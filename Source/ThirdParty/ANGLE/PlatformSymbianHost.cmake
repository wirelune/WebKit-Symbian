# The Symbian port's Windows test host: ANGLE's own EGL and GLES2 on Direct3D 11
# (as PlatformWin.cmake), standing in for the phone's OpenGL ES 2.0 drivers.
list(APPEND ANGLE_DEFINITIONS
    GL_APICALL=
    GL_API=
    NOMINMAX
)

if (USE_ANGLE_EGL)
    list(APPEND ANGLE_SOURCES
        ${angle_translator_hlsl_sources}

        ${_d3d_shared_sources}
        ${_d3d11_backend_sources}
        ${libangle_gpu_info_util_sources}
        ${libangle_gpu_info_util_win_sources}
    )

    list(APPEND ANGLE_DEFINITIONS
        ANGLE_ENABLE_HLSL
        ANGLE_ENABLE_D3D11
    )

    find_package(ZLIB REQUIRED)
    list(APPEND ANGLEGLESv2_LIBRARIES dxguid dxgi ZLIB::ZLIB)
endif ()
