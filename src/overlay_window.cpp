#include "overlay_window.h"

#include "crosshair_renderer.h"

#include <objidl.h>
#include <gdiplus.h>

namespace potato
{
namespace
{
constexpr UINT kHotkeyToggle = 1;
constexpr UINT kHotkeyStyle = 2;
constexpr UINT kHotkeyColor = 3;
constexpr UINT kHotkeyLengthUp = 4;
constexpr UINT kHotkeyLengthDown = 5;
constexpr UINT kHotkeyGapUp = 6;
constexpr UINT kHotkeyGapDown = 7;
constexpr UINT kHotkeyThicknessUp = 8;
constexpr UINT kHotkeyThicknessDown = 9;
constexpr UINT kHotkeyOpacityUp = 10;
constexpr UINT kHotkeyOpacityDown = 11;
constexpr UINT kHotkeyReset = 12;
constexpr UINT kHotkeyQuit = 13;

MONITORINFO GetPrimaryMonitorInfo()
{
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const POINT anchor{0, 0};
    const HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(monitor, &monitorInfo);
    return monitorInfo;
}
} // namespace

bool OverlayWindow::Create(const HINSTANCE instance)
{
    instance_ = instance;

    Gdiplus::GdiplusStartupInput startupInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &startupInput, nullptr) != Gdiplus::Ok)
    {
        return false;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &OverlayWindow::WindowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kOverlayWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;

    RegisterClassExW(&windowClass);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kOverlayWindowClass,
        kAppName,
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        64,
        64,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ == nullptr)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);
    return true;
}

void OverlayWindow::Destroy()
{
    if (hwnd_ != nullptr)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    if (gdiplusToken_ != 0)
    {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
}

void OverlayWindow::ApplySettings(const Settings& settings)
{
    settings_ = settings;
    RefreshOverlayPlacement();
}

void OverlayWindow::SetSettingsChangedCallback(SettingsChangedCallback callback)
{
    settingsChangedCallback_ = std::move(callback);
}

void OverlayWindow::SetQuitRequestedCallback(QuitRequestedCallback callback)
{
    quitRequestedCallback_ = std::move(callback);
}

HWND OverlayWindow::hwnd() const
{
    return hwnd_;
}

LRESULT CALLBACK OverlayWindow::WindowProc(const HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* window = static_cast<OverlayWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
        return TRUE;
    }

    auto* window = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window != nullptr)
    {
        return window->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        RegisterHotkeys();
        RefreshOverlayPlacement();
        return 0;

    case WM_HOTKEY:
        HandleHotkey(static_cast<UINT>(wParam));
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        RefreshOverlayPlacement();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_DESTROY:
        UnregisterHotkeys();
        return 0;

    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void OverlayWindow::RegisterHotkeys()
{
    constexpr UINT modifiers = MOD_ALT | MOD_CONTROL | MOD_NOREPEAT;
    RegisterHotKey(hwnd_, kHotkeyToggle, MOD_NOREPEAT, VK_F8);
    RegisterHotKey(hwnd_, kHotkeyStyle, MOD_NOREPEAT, VK_F9);
    RegisterHotKey(hwnd_, kHotkeyColor, modifiers, 'C');
    RegisterHotKey(hwnd_, kHotkeyLengthUp, modifiers, VK_UP);
    RegisterHotKey(hwnd_, kHotkeyLengthDown, modifiers, VK_DOWN);
    RegisterHotKey(hwnd_, kHotkeyGapDown, modifiers, VK_LEFT);
    RegisterHotKey(hwnd_, kHotkeyGapUp, modifiers, VK_RIGHT);
    RegisterHotKey(hwnd_, kHotkeyThicknessUp, modifiers, VK_PRIOR);
    RegisterHotKey(hwnd_, kHotkeyThicknessDown, modifiers, VK_NEXT);
    RegisterHotKey(hwnd_, kHotkeyOpacityUp, modifiers, VK_HOME);
    RegisterHotKey(hwnd_, kHotkeyOpacityDown, modifiers, VK_END);
    RegisterHotKey(hwnd_, kHotkeyReset, modifiers, 'R');
    RegisterHotKey(hwnd_, kHotkeyQuit, modifiers, 'Q');
}

void OverlayWindow::UnregisterHotkeys() const
{
    for (UINT hotkey = kHotkeyToggle; hotkey <= kHotkeyQuit; ++hotkey)
    {
        UnregisterHotKey(hwnd_, hotkey);
    }
}

void OverlayWindow::HandleHotkey(const UINT hotkey)
{
    bool requiresRelayout = false;

    switch (hotkey)
    {
    case kHotkeyToggle:
        settings_.visible = !settings_.visible;
        break;

    case kHotkeyStyle:
        SwitchStyleProfile(settings_, IntToStyle((StyleToInt(settings_.style) + 1) % kCrosshairStyleCount));
        requiresRelayout = true;
        break;

    case kHotkeyColor:
        settings_.colorIndex = (settings_.colorIndex + 1) % static_cast<int>(kColorPresets.size());
        settings_.useCustomColor = false;
        break;

    case kHotkeyLengthUp:
        settings_.length = ClampInt(settings_.length + 2, 4, 120);
        requiresRelayout = true;
        break;

    case kHotkeyLengthDown:
        settings_.length = ClampInt(settings_.length - 2, 4, 120);
        requiresRelayout = true;
        break;

    case kHotkeyGapUp:
        settings_.gap = ClampInt(settings_.gap + 1, 0, 80);
        requiresRelayout = true;
        break;

    case kHotkeyGapDown:
        settings_.gap = ClampInt(settings_.gap - 1, 0, 80);
        requiresRelayout = true;
        break;

    case kHotkeyThicknessUp:
        settings_.thickness = ClampInt(settings_.thickness + 1, 1, 20);
        requiresRelayout = true;
        break;

    case kHotkeyThicknessDown:
        settings_.thickness = ClampInt(settings_.thickness - 1, 1, 20);
        requiresRelayout = true;
        break;

    case kHotkeyOpacityUp:
        settings_.opacity = ClampInt(settings_.opacity + 10, 40, 255);
        break;

    case kHotkeyOpacityDown:
        settings_.opacity = ClampInt(settings_.opacity - 10, 40, 255);
        break;

    case kHotkeyReset:
        settings_ = Settings{};
        requiresRelayout = true;
        break;

    case kHotkeyQuit:
        if (quitRequestedCallback_)
        {
            quitRequestedCallback_();
        }
        return;

    default:
        return;
    }

    if (requiresRelayout)
    {
        RefreshOverlayPlacement();
    }
    else
    {
        Render();
    }

    if (settingsChangedCallback_)
    {
        settingsChangedCallback_(settings_);
    }
}

void OverlayWindow::RefreshOverlayPlacement()
{
    if (hwnd_ == nullptr)
    {
        return;
    }

    const MONITORINFO monitorInfo = GetPrimaryMonitorInfo();
    const int windowSize = CalculateOverlayWindowSize(settings_);
    const int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    const int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    const int x = monitorInfo.rcMonitor.left + (monitorWidth / 2) - (windowSize / 2);
    const int y = monitorInfo.rcMonitor.top + (monitorHeight / 2) - (windowSize / 2);

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, windowSize, windowSize, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    Render();
}

void OverlayWindow::Render() const
{
    if (hwnd_ == nullptr)
    {
        return;
    }

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const HDC screenDc = GetDC(nullptr);
    const HDC memoryDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    const HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
    const HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);

    {
        Gdiplus::Graphics graphics(memoryDc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

        if (settings_.visible)
        {
            DrawCrosshair(graphics, settings_, Gdiplus::RectF(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)));
        }
    }

    POINT destination{};
    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);
    destination.x = windowRect.left;
    destination.y = windowRect.top;

    SIZE size{width, height};
    POINT source{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd_, screenDc, &destination, &size, memoryDc, &source, 0, &blend, ULW_ALPHA);

    SelectObject(memoryDc, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
}
} // namespace potato
