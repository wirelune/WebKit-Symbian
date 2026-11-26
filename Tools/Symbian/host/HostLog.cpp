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

#include "HostLog.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

void HostLogOpen(const char* directory)
{
    std::string path = std::string(directory) + "/log.txt";
    std::string oldPath = std::string(directory) + "/log.old.txt";
    remove(oldPath.c_str());
    rename(path.c_str(), oldPath.c_str());
    if (!freopen(path.c_str(), "w", stderr))
        return;
    // Line buffered, so a crash loses at most the line being written.
    setvbuf(stderr, nullptr, _IOLBF, 512);
}

void HostLog(const char* format, ...)
{
    char message[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    SYSTEMTIME time;
    GetLocalTime(&time);
    fprintf(stderr, "%02d:%02d:%02d.%03d %s\n", time.wHour, time.wMinute, time.wSecond,
        time.wMilliseconds, message);
    fflush(stderr);
}
