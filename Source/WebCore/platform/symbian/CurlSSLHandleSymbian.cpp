/*
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurlSSLHandle.h"

#include <stdlib.h>
#include <wtf/FileSystem.h>

#if OS(WINDOWS)
#include <windows.h>
#endif

namespace WebCore {

// Trusted roots in PEM, installed with the browser; CURL_CA_BUNDLE overrides.
// On the phone they are in C:/resource/webkit; the Windows test host has
// them next to its exe.
static String defaultCACertPath()
{
#if OS(WINDOWS)
    wchar_t module[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    return FileSystem::pathByAppendingComponent(
        FileSystem::directoryName(String(reinterpret_cast<const UChar*>(module), length)), "cacert.pem");
#else
    return "C:/resource/webkit/cacert.pem"_s;
#endif
}

void CurlSSLHandle::platformInitialize()
{
    const char* path = getenv("CURL_CA_BUNDLE");
    setCACertPath(path && *path ? String::fromUTF8(path) : defaultCACertPath());
}

} // namespace WebCore
