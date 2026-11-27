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

#include "HostCrash.h"

#include "HostLog.h"

#include <windows.h>
#include <dbghelp.h>
#include <string>

static std::string crashDumpPath;

static LONG WINAPI crashFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    EXCEPTION_RECORD* record = exceptionInfo->ExceptionRecord;
    HostLog("crash: exception %08lx at %p, thread %lu", static_cast<unsigned long>(record->ExceptionCode),
        record->ExceptionAddress, static_cast<unsigned long>(GetCurrentThreadId()));

    HANDLE file = CreateFileA(crashDumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION information { GetCurrentThreadId(), exceptionInfo, FALSE };
        BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
            MiniDumpNormal, &information, nullptr, nullptr);
        CloseHandle(file);
        HostLog("crash: minidump %s (%s)", written ? "written" : "failed", crashDumpPath.c_str());
    } else
        HostLog("crash: no minidump, %s cannot be created", crashDumpPath.c_str());

    // Let the CRT run its own handler, so the exit status still says what
    // happened.
    return EXCEPTION_CONTINUE_SEARCH;
}

void HostInstallCrashHandler(const char* directory)
{
    crashDumpPath = std::string(directory) + "/crash.dmp";
    SetUnhandledExceptionFilter(crashFilter);
}
