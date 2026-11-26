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

#pragma once

#include <WebKitSymbian.h>
#include <windows.h>
#include <memory>
#include <vector>

// The test host's window: an address field over a view the size of the
// phone's (360x510, as log.txt reports on the N8). The page paints into a
// buffer of the view's size, as it paints into the phone engine's chunk, and
// the mouse acts as a finger.
class HostWindow final : public WebKitSymbian::WebViewClient {
public:
    static constexpr int viewWidth = 360;
    static constexpr int viewHeight = 510;
    static constexpr int addressHeight = 28;
    static constexpr int addressId = 1;

    HostWindow();
    ~HostWindow();

    void load(const char* url);
    // Paints the whole view and writes it as a 32-bit BMP.
    bool saveBitmap(const wchar_t* path);
    HWND handle() const { return m_window; }
    WebKitSymbian::WebView& view() { return *m_view; }

    // WebViewClient
    void didInvalidate(const WebKitSymbian::Rect&) override;
    void didChangeTitle(WebKitSymbian::Text) override;
    void didChangeURL(WebKitSymbian::Text) override;
    void runJavaScriptAlert(WebKitSymbian::Text) override;
    bool runJavaScriptConfirm(WebKitSymbian::Text) override;

private:
    static LRESULT CALLBACK windowProcedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT, WPARAM, LPARAM);
    void paint();
    void paintView(const WebKitSymbian::Rect&);

    HWND m_window { nullptr };
    HWND m_address { nullptr };
    std::unique_ptr<WebKitSymbian::WebView> m_view;
    std::vector<uint32_t> m_pixels;
    bool m_pointerDown { false };
};
