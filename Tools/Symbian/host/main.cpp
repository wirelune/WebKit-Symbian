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

// wkhost.exe: the Symbian port's engine on Windows, for debugging pages as
// the phone shows them. See Tools/Symbian/docs/WindowsHost-design.md.
//
//   wkhost.exe [URL] [--data DIR] [--shot FILE.bmp] [--wait SECONDS]
//
// --shot loads the page, waits (10 seconds by default), saves the view and
// exits: the CI's smoke test.

#include "HostLog.h"
#include "HostSettings.h"
#include "HostWindow.h"

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>

using namespace WebKitSymbian;

static const UINT wakeUpMessage = WM_APP + 1;
static DWORD mainThread;
static UINT_PTR engineTimer;

// The engine calls this from any thread when it has work; as on the phone,
// it only signals the event loop, and the work runs on the main thread.
static void wakeUp(void*)
{
    PostThreadMessageW(mainThread, wakeUpMessage, 0, 0);
}

static void runEngine()
{
    WebView::runPendingWork();
    int64_t next = WebView::microsecondsUntilNextTimer();
    if (engineTimer) {
        KillTimer(nullptr, engineTimer);
        engineTimer = 0;
    }
    if (next >= 0)
        engineTimer = SetTimer(nullptr, 0, static_cast<UINT>(next / 1000) + 1, nullptr);
}

static std::string toUTF8(const std::wstring& text)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(length > 0 ? length - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length, nullptr, nullptr);
    return result;
}

static std::wstring toWide(const std::string& text)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(length > 0 ? length - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), length);
    return result;
}

// %LOCALAPPDATA%\WebKitQt, laid out as C:\Data\WebKitQt on the phone.
static std::string defaultDataDirectory()
{
    wchar_t* local = nullptr;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local);
    std::wstring path = std::wstring(local) + L"\\WebKitQt";
    CoTaskMemFree(local);
    return toUTF8(path);
}

int wmain(int argc, wchar_t** argv)
{
    mainThread = GetCurrentThreadId();
    std::string url = "https://duckduckgo.com/lite/";
    std::string data;
    std::wstring shot;
    int wait = 10;
    for (int i = 1; i < argc; ++i) {
        std::wstring argument = argv[i];
        if (argument == L"--data" && i + 1 < argc)
            data = toUTF8(argv[++i]);
        else if (argument == L"--shot" && i + 1 < argc)
            shot = argv[++i];
        else if (argument == L"--wait" && i + 1 < argc)
            wait = _wtoi(argv[++i]);
        else if (argument.rfind(L"--", 0) != 0)
            url = toUTF8(argument);
    }
    if (data.empty())
        data = defaultDataDirectory();
    for (auto& character : data) {
        if (character == '\\')
            character = '/';
    }
    SHCreateDirectoryExW(nullptr, toWide(data).c_str(), nullptr);

    // curl's trace goes to curl.txt while the file curl-verbose exists, as on the phone.
    std::string curlTrace = data + "/curl.txt";
    if (GetFileAttributesW(toWide(data + "/curl-verbose").c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileW(toWide(curlTrace).c_str());
        _putenv_s("DEBUG_CURL", "1");
        _putenv_s("CURL_LOG_FILE", curlTrace.c_str());
    }

    HostLogOpen(data.c_str());
    HostLog("WebKit engine starting");
    WebView::initializeProcess(data.c_str());
    HostSettings settings = HostReadSettings(data);
    std::vector<const char*> languages;
    for (auto& language : settings.languages)
        languages.push_back(language.c_str());
    WebView::setLanguages(languages.data(), static_cast<int>(languages.size()));
    HostLog("languages: %s, %s", languages[0], languages[1]);
    if (!settings.userAgent.empty()) {
        WebView::setUserAgent(settings.userAgent.c_str());
        HostLog("user agent: %s", settings.userAgent.c_str());
    }
    // Messages for the main thread need its queue to exist before anything posts.
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    WebView::setWakeUpCallback(wakeUp, nullptr);
    HostLog("engine initialized");

    HostWindow window;
    window.load(url.c_str());
    runEngine();
    ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(wait) * 1000;

    for (;;) {
        DWORD timeout = INFINITE;
        if (!shot.empty()) {
            ULONGLONG now = GetTickCount64();
            timeout = deadline > now ? static_cast<DWORD>(deadline - now) : 0;
        }
        if (MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout, QS_ALLINPUT) == WAIT_TIMEOUT) {
            runEngine();
            bool saved = window.saveBitmap(shot.c_str());
            HostLog("screenshot %s, exiting", saved ? "saved" : "failed");
            return saved ? 0 : 1;
        }
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                return 0;
            if (message.message == wakeUpMessage || (message.message == WM_TIMER && !message.hwnd)) {
                runEngine();
                continue;
            }
            if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN
                && GetDlgCtrlID(message.hwnd) == HostWindow::addressId) {
                wchar_t text[2048];
                GetWindowTextW(message.hwnd, text, 2048);
                window.load(toUTF8(text).c_str());
                SetFocus(window.handle());
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}
