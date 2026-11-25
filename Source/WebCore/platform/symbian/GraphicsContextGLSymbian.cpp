/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2026 The symbian-webkit authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// WebGL 1 on the phone's OpenGL ES 2.0 (Broadcom VideoCore on Symbian^3).
// The engine has no window and nothing composites, so each WebGL context
// gets its own ES 2.0 context on a 1x1 pbuffer and draws into the FBO that
// GraphicsContextGLOpenGLES.cpp manages; the canvas reads the drawing back
// when it paints (GraphicsContextGLOpenGL.cpp, GraphicsContextGLCairo.cpp).
// Shaders go through ANGLE's translator (ANGLEWebKitBridge) to ESSL.

#include "config.h"
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(WEBGL) && OS(SYMBIAN) && !USE(ANGLE)

#include <ANGLE/ShaderLang.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wtf/Deque.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Each context holds GPU memory, of which the phone has little; the oldest
// is lost when a page makes more.
static const size_t MaxActiveContexts = 4;

static Deque<GraphicsContextGLOpenGL*, MaxActiveContexts>& activeContexts()
{
    static NeverDestroyed<Deque<GraphicsContextGLOpenGL*, MaxActiveContexts>> contexts;
    return contexts;
}

// The context made current last. Every GL call makes its context current,
// so the check has to be cheap; nothing else in the engine uses EGL.
static GraphicsContextGLOpenGL* s_currentContext;

namespace {

struct Display {
    EGLDisplay display { EGL_NO_DISPLAY };
    EGLConfig config { nullptr };
};

}

// The display and the config the contexts share, set up once. Null when the
// phone has no OpenGL ES 2.0 with pbuffers.
static const Display* display()
{
    static Display shared;
    static bool initialized;
    if (initialized)
        return shared.display != EGL_NO_DISPLAY ? &shared : nullptr;
    initialized = true;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) {
        WTFLogAlways("webgl: no EGL display (0x%x), WebGL is off", eglGetError());
        return nullptr;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        WTFLogAlways("webgl: no OpenGL ES in EGL (0x%x), WebGL is off", eglGetError());
        eglTerminate(display);
        return nullptr;
    }

    // WebGL draws into its own framebuffer; the pbuffer only lets the
    // context be current.
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(display, attributes, &config, 1, &count) || count < 1) {
        WTFLogAlways("webgl: no OpenGL ES 2.0 pbuffer config (0x%x), WebGL is off", eglGetError());
        eglTerminate(display);
        return nullptr;
    }

    WTFLogAlways("webgl: EGL %d.%d, %s", major, minor, eglQueryString(display, EGL_VENDOR));
    shared.display = display;
    shared.config = config;
    return &shared;
}

RefPtr<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::create(GraphicsContextGLAttributes attributes, HostWindow* hostWindow)
{
    if (!display())
        return nullptr;

    auto& contexts = activeContexts();
    if (contexts.size() >= MaxActiveContexts)
        contexts.first()->recycleContext();

    // Recycling the oldest context should have destroyed it, which takes it
    // off the list.
    if (contexts.size() >= MaxActiveContexts)
        return nullptr;

    auto context = adoptRef(new GraphicsContextGLOpenGL(attributes, hostWindow));
    if (!context->m_eglContext)
        return nullptr;

    contexts.append(context.get());
    return context;
}

GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes, HostWindow*, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attributes, sharedContext)
{
    ASSERT_UNUSED(sharedContext, !sharedContext);

    const Display* shared = display();
    if (!shared)
        return;

    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(shared->display, shared->config, EGL_NO_CONTEXT, contextAttributes);
    if (context == EGL_NO_CONTEXT) {
        WTFLogAlways("webgl: eglCreateContext failed (0x%x)", eglGetError());
        return;
    }
    const EGLint pbufferAttributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(shared->display, shared->config, pbufferAttributes);
    if (surface == EGL_NO_SURFACE) {
        WTFLogAlways("webgl: eglCreatePbufferSurface failed (0x%x)", eglGetError());
        eglDestroyContext(shared->display, context);
        return;
    }
    m_eglContext = context;
    m_eglSurface = surface;

    if (!makeContextCurrent()) {
        WTFLogAlways("webgl: eglMakeCurrent failed (0x%x)", eglGetError());
        eglDestroySurface(shared->display, surface);
        eglDestroyContext(shared->display, context);
        m_eglContext = nullptr;
        m_eglSurface = nullptr;
        return;
    }

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    // The texture and the framebuffer the page draws into; reshape() sizes
    // them (GraphicsContextGLOpenGLES.cpp).
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ::glBindTexture(GL_TEXTURE_2D, 0);

    ::glGenFramebuffers(1, &m_fbo);
    ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        ::glGenFramebuffers(1, &m_multisampleFBO);
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        ::glGenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            ::glGenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
        // Without OES_packed_depth_stencil depth and stencil are separate.
        if (attributes.depth)
            ::glGenRenderbuffers(1, &m_depthBuffer);
        if (attributes.stencil)
            ::glGenRenderbuffers(1, &m_stencilBuffer);
        if (attributes.stencil || attributes.depth)
            ::glGenRenderbuffers(1, &m_depthStencilBuffer);
    }

    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, SH_WEBGL_SPEC);

    ShBuiltInResources resources;
    sh::InitBuiltInResources(&resources);
    resources.MaxVertexAttribs = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_ATTRIBS);
    resources.MaxVertexUniformVectors = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_UNIFORM_VECTORS);
    resources.MaxVaryingVectors = getInteger(GraphicsContextGLOpenGL::MAX_VARYING_VECTORS);
    resources.MaxVertexTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    resources.MaxCombinedTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    resources.MaxTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_TEXTURE_IMAGE_UNITS);
    resources.MaxFragmentUniformVectors = getInteger(GraphicsContextGLOpenGL::MAX_FRAGMENT_UNIFORM_VECTORS);
    // Always 1 for OpenGL ES 2.0.
    resources.MaxDrawBuffers = 1;

    GCGLint range[2] { };
    GCGLint precision = 0;
    getShaderPrecisionFormat(GraphicsContextGLOpenGL::FRAGMENT_SHADER, GraphicsContextGLOpenGL::HIGH_FLOAT, range, &precision);
    resources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(resources);

    ::glClearColor(0, 0, 0, 0);
}

GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    activeContexts().takeLast([this](auto* it) {
        return it == this;
    });

    const Display* shared = display();
    if (!m_eglContext || !shared)
        return;

    if (makeContextCurrent()) {
        if (m_texture)
            ::glDeleteTextures(1, &m_texture);

        auto attributes = contextAttributes();
        if (attributes.antialias) {
            ::glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
            if (attributes.stencil || attributes.depth)
                ::glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
            ::glDeleteFramebuffers(1, &m_multisampleFBO);
        } else if (attributes.stencil || attributes.depth) {
            if (m_depthBuffer)
                ::glDeleteRenderbuffers(1, &m_depthBuffer);
            if (m_stencilBuffer)
                ::glDeleteRenderbuffers(1, &m_stencilBuffer);
            if (m_depthStencilBuffer)
                ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
        }
        ::glDeleteFramebuffers(1, &m_fbo);
        if (m_vao)
            deleteVertexArray(m_vao);
    }

    eglMakeCurrent(shared->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    s_currentContext = nullptr;
    eglDestroySurface(shared->display, static_cast<EGLSurface>(m_eglSurface));
    eglDestroyContext(shared->display, static_cast<EGLContext>(m_eglContext));
}

bool GraphicsContextGLOpenGL::makeContextCurrent()
{
    if (!m_eglContext)
        return false;
    if (s_currentContext == this)
        return true;
    const Display* shared = display();
    if (!shared)
        return false;
    EGLSurface surface = static_cast<EGLSurface>(m_eglSurface);
    if (!eglMakeCurrent(shared->display, surface, surface, static_cast<EGLContext>(m_eglContext))) {
        s_currentContext = nullptr;
        return false;
    }
    s_currentContext = this;
    return true;
}

void GraphicsContextGLOpenGL::checkGPUStatus()
{
}

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
    return true;
}

PlatformLayer* GraphicsContextGLOpenGL::platformLayer() const
{
    // Nothing composites: the canvas paints the drawing it reads back.
    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && OS(SYMBIAN) && !USE(ANGLE)
