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

#include "HostWindow.h"

#include "HostLog.h"
#include <stdio.h>
#include <string>
#include <windowsx.h>

using namespace WebKitSymbian;

static const wchar_t className[] = L"WebKitSymbianHost";

static std::wstring toWide(Text text)
{
    return std::wstring(reinterpret_cast<const wchar_t*>(text.characters), text.length);
}

HostWindow::HostWindow()
    : m_pixels(viewWidth * viewHeight, 0xffffffff)
{
    WNDCLASSW windowClass { };
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    RECT frame { 0, 0, viewWidth, viewHeight + addressHeight };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&frame, style, FALSE);
    m_window = CreateWindowW(className, L"WebKit Symbian host", style, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, windowClass.hInstance, this);
    m_address = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, viewWidth, addressHeight, m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(addressId)),
        windowClass.hInstance, nullptr);
    m_view.reset(WebView::create(*this, viewWidth, viewHeight));
    m_view->setFocused(true);
    HostLog("view %dx%d", viewWidth, viewHeight);
    ShowWindow(m_window, SW_SHOW);
}

HostWindow::~HostWindow() = default;

void HostWindow::load(const char* url)
{
    HostLog("load %s", url);
    m_view->load(url);
}

void HostWindow::didInvalidate(const Rect& area)
{
    RECT rect { area.x, area.y + addressHeight, area.x + area.width, area.y + area.height + addressHeight };
    InvalidateRect(m_window, &rect, FALSE);
}

void HostWindow::didChangeTitle(Text title)
{
    SetWindowTextW(m_window, toWide(title).c_str());
}

void HostWindow::didChangeURL(Text url)
{
    SetWindowTextW(m_address, toWide(url).c_str());
}

void HostWindow::runJavaScriptAlert(Text message)
{
    MessageBoxW(m_window, toWide(message).c_str(), L"Alert", MB_OK);
}

bool HostWindow::runJavaScriptConfirm(Text message)
{
    return MessageBoxW(m_window, toWide(message).c_str(), L"Confirm", MB_OKCANCEL) == IDOK;
}

void HostWindow::paintView(const Rect& area)
{
    m_view->paint(reinterpret_cast<uint8_t*>(m_pixels.data()), viewWidth * 4, area);
}

void HostWindow::paint()
{
    PAINTSTRUCT paintStruct;
    HDC context = BeginPaint(m_window, &paintStruct);
    RECT dirty = paintStruct.rcPaint;
    Rect area { dirty.left, dirty.top - addressHeight, dirty.right - dirty.left, dirty.bottom - dirty.top };
    if (area.y < 0) {
        area.height += area.y;
        area.y = 0;
    }
    if (area.width > 0 && area.height > 0) {
        paintView(area);
        // Premultiplied ARGB words are BGRA bytes, which is what a 32-bit DIB holds.
        BITMAPINFO info { };
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = viewWidth;
        info.bmiHeader.biHeight = -viewHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(context, 0, addressHeight, viewWidth, viewHeight, 0, 0, 0, viewHeight,
            m_pixels.data(), &info, DIB_RGB_COLORS);
    }
    EndPaint(m_window, &paintStruct);
}

bool HostWindow::saveBitmap(const wchar_t* path)
{
    paintView({ 0, 0, viewWidth, viewHeight });
    BITMAPFILEHEADER file { };
    BITMAPINFOHEADER info { };
    info.biSize = sizeof(info);
    info.biWidth = viewWidth;
    info.biHeight = -viewHeight;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    DWORD size = static_cast<DWORD>(m_pixels.size() * 4);
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + size;
    FILE* out = _wfopen(path, L"wb");
    if (!out)
        return false;
    fwrite(&file, sizeof(file), 1, out);
    fwrite(&info, sizeof(info), 1, out);
    fwrite(m_pixels.data(), 1, size, out);
    fclose(out);
    return true;
}

LRESULT CALLBACK HostWindow::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<HostWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!self || !self->m_window || !self->m_view)
        return DefWindowProcW(window, message, wParam, lParam);
    return self->handleMessage(message, wParam, lParam);
}

static unsigned modifiers()
{
    unsigned result = 0;
    if (GetKeyState(VK_SHIFT) < 0)
        result |= ShiftKey;
    if (GetKeyState(VK_CONTROL) < 0)
        result |= ControlKey;
    if (GetKeyState(VK_MENU) < 0)
        result |= AltKey;
    return result;
}

LRESULT HostWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam) - addressHeight;
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_LBUTTONDOWN:
        if (y < 0)
            break;
        SetFocus(m_window);
        SetCapture(m_window);
        m_pointerDown = true;
        m_view->handlePointer(PointerEvent::Down, x, y, 0);
        return 0;
    case WM_MOUSEMOVE:
        if (m_pointerDown)
            m_view->handlePointer(PointerEvent::Move, x, y, 0);
        return 0;
    case WM_LBUTTONUP:
        if (!m_pointerDown)
            break;
        m_pointerDown = false;
        ReleaseCapture();
        m_view->handlePointer(PointerEvent::Up, x, y, 0);
        return 0;
    case WM_MOUSEWHEEL:
        m_view->scrollBy(0, -GET_WHEEL_DELTA_WPARAM(wParam) / 2);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
        m_view->handleKey(message == WM_KEYDOWN, static_cast<int>(wParam), { }, modifiers());
        return 0;
    case WM_CHAR: {
        uint16_t character = static_cast<uint16_t>(wParam);
        if (character >= 0x20)
            m_view->confirmComposition({ &character, 1 });
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_window, message, wParam, lParam);
}
