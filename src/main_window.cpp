#include "main_window.h"

#include "app_controller.h"
#include "crosshair_renderer.h"
#include "resource.h"
#include "tray_icon.h"

#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <windowsx.h>

#include <array>
#include <sstream>

namespace potato
{
namespace
{
constexpr int kWindowWidth = 980;
constexpr int kWindowHeight = 680;
constexpr int kWindowMinWidth = 900;
constexpr int kWindowMinHeight = 680;
constexpr int kHeaderHeight = 78;
constexpr int kSidebarWidth = 190;
constexpr int kSidebarButtonHeight = 42;
constexpr int kSidebarGap = 10;
constexpr int kContentGap = 20;
constexpr int kResizeBorder = 8;
constexpr int kWindowCornerRadius = 20;
constexpr int kStyleButtonHeight = 40;
constexpr int kToggleHeight = 30;
constexpr int kToggleTrackWidth = 50;
constexpr int kToggleTrackHeight = 22;
constexpr int kToggleThumbInset = 3;
constexpr int kCardPadding = 24;
constexpr int kCardTitleHeight = 52;
constexpr int kPreviewPreferredSize = 240;
constexpr int kCrosshairStackBreakpoint = 620;
constexpr int kSliderHorizontalPadding = 18;
constexpr int kSliderTrackHeight = 4;
constexpr int kSliderThumbRadius = 9;
constexpr UINT kSliderChangedMessage = WM_APP + 141;
constexpr UINT kSliderSetRangeMessage = WM_APP + 142;
constexpr UINT kSliderSetValueMessage = WM_APP + 143;
constexpr wchar_t kSliderWindowClass[] = L"PotatoCrosshairSliderWindow";

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

RECT MakeRect(const int left, const int top, const int width, const int height)
{
    RECT rect{left, top, left + width, top + height};
    return rect;
}

int RectWidth(const RECT& rect)
{
    return rect.right - rect.left;
}

int RectHeight(const RECT& rect)
{
    return rect.bottom - rect.top;
}

std::wstring SliderText(const wchar_t* label, const int value, const wchar_t* suffix = L"")
{
    std::wostringstream stream;
    stream << label << value << suffix;
    return stream.str();
}

std::wstring FileLabelText(const Settings& settings)
{
    if (settings.customImagePath.empty())
    {
        return L"No PNG loaded.";
    }

    std::wstring filename = settings.customImagePath;
    const size_t slashIndex = filename.find_last_of(L"\\/");
    if (slashIndex != std::wstring::npos)
    {
        filename = filename.substr(slashIndex + 1);
    }

    return L"PNG file: " + filename;
}

RECT InflateRectCopy(RECT rect, const int amount)
{
    rect.left -= amount;
    rect.top -= amount;
    rect.right += amount;
    rect.bottom += amount;
    return rect;
}

void DrawCard(HDC dc, const RECT& rect)
{
    const HBRUSH brush = CreateSolidBrush(kThemePanelBackground);
    const HPEN pen = CreatePen(PS_SOLID, 1, kThemeBorder);
    const HGDIOBJ previousBrush = SelectObject(dc, brush);
    const HGDIOBJ previousPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 16, 16);
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void FillRoundRect(HDC dc, const RECT& rect, const int radius, const COLORREF fillColor, const COLORREF borderColor)
{
    const HBRUSH brush = CreateSolidBrush(fillColor);
    const HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    const HGDIOBJ previousBrush = SelectObject(dc, brush);
    const HGDIOBJ previousPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

struct CrosshairSectionLayout
{
    RECT previewCard{};
    RECT controlsCard{};
    RECT previewRect{};
    bool stacked = false;
};

CrosshairSectionLayout CalculateCrosshairSectionLayout(const int cardLeft, const int sectionTop, const int availableWidth, const int sectionHeight)
{
    CrosshairSectionLayout layout{};
    layout.stacked = availableWidth < kCrosshairStackBreakpoint;

    const int previewCardWidth = layout.stacked ? availableWidth : std::min(356, std::max(296, availableWidth / 3));
    const int controlsCardWidth = layout.stacked ? availableWidth : availableWidth - previewCardWidth - kContentGap;
    const int previewCardHeight = layout.stacked ? 290 : sectionHeight;
    const int controlsCardTop = layout.stacked ? sectionTop + previewCardHeight + kContentGap : sectionTop;
    const int controlsCardHeight = layout.stacked ? sectionHeight - previewCardHeight - kContentGap : sectionHeight;
    const int controlsLeft = layout.stacked ? cardLeft : cardLeft + previewCardWidth + kContentGap;

    layout.previewCard = MakeRect(cardLeft, sectionTop, previewCardWidth, previewCardHeight);
    layout.controlsCard = MakeRect(controlsLeft, controlsCardTop, controlsCardWidth, controlsCardHeight);

    const int previewSpaceWidth = RectWidth(layout.previewCard) - (kCardPadding * 2);
    const int previewSpaceHeight = RectHeight(layout.previewCard) - kCardTitleHeight - (kCardPadding * 2);
    const int previewBase = std::min(previewSpaceWidth, previewSpaceHeight);
    const int previewEdge = std::max(1, std::min(std::max(kPreviewPreferredSize, previewBase - 12), previewBase));
    const int previewLeft = layout.previewCard.left + (RectWidth(layout.previewCard) - previewEdge) / 2;
    const int previewTop = layout.previewCard.top + kCardTitleHeight + kCardPadding + std::max(0, (previewSpaceHeight - previewEdge) / 2);
    layout.previewRect = MakeRect(previewLeft, previewTop, previewEdge, previewEdge);
    return layout;
}

bool IsToggleControlId(const int controlId)
{
    return controlId == ID_GENERAL_VISIBLE
        || controlId == ID_USE_CUSTOM_COLOR
        || controlId == ID_ENABLE_CUSTOM_IMAGE
        || controlId == ID_MIDDLE_DOT
        || controlId == ID_OUTLINE_ENABLED;
}

bool ToggleStateForControl(const Settings& settings, const int controlId)
{
    switch (controlId)
    {
    case ID_GENERAL_VISIBLE:
        return settings.visible;
    case ID_USE_CUSTOM_COLOR:
        return settings.useCustomColor;
    case ID_ENABLE_CUSTOM_IMAGE:
        return settings.renderMode == RenderMode::CustomImage && !settings.customImagePath.empty();
    case ID_MIDDLE_DOT:
        return settings.middleDot;
    case ID_OUTLINE_ENABLED:
        return settings.outlineEnabled;
    default:
        return false;
    }
}

struct SliderState
{
    int minimum = 0;
    int maximum = 100;
    int value = 0;
    bool dragging = false;
};

RECT SliderTrackRect(const RECT& clientRect)
{
    const int centerY = (clientRect.bottom - clientRect.top) / 2;
    const int trackWidth = std::max(1, static_cast<int>(clientRect.right - clientRect.left) - (kSliderHorizontalPadding * 2));
    return MakeRect(clientRect.left + kSliderHorizontalPadding, centerY - (kSliderTrackHeight / 2), trackWidth, kSliderTrackHeight);
}

int SliderThumbCenterX(const RECT& clientRect, const SliderState& state)
{
    const RECT trackRect = SliderTrackRect(clientRect);
    const int range = std::max(1, state.maximum - state.minimum);
    const double ratio = static_cast<double>(state.value - state.minimum) / static_cast<double>(range);
    return trackRect.left + static_cast<int>(ratio * static_cast<double>(trackRect.right - trackRect.left));
}

int SliderValueFromPoint(const RECT& clientRect, const SliderState& state, const int x)
{
    const RECT trackRect = SliderTrackRect(clientRect);
    const int clampedX = ClampInt(x, trackRect.left, trackRect.right);
    const int trackWidth = std::max(1, static_cast<int>(trackRect.right - trackRect.left));
    const double ratio = static_cast<double>(clampedX - trackRect.left) / static_cast<double>(trackWidth);
    return state.minimum + static_cast<int>(ratio * static_cast<double>(state.maximum - state.minimum) + 0.5);
}

void PaintSlider(const HWND hwnd, SliderState& state)
{
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(hwnd, &paint);

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;

    const HDC bufferDc = CreateCompatibleDC(dc);
    const HBITMAP bufferBitmap = CreateCompatibleBitmap(dc, std::max(1, width), std::max(1, height));
    const HGDIOBJ previousBitmap = SelectObject(bufferDc, bufferBitmap);

    const HBRUSH backgroundBrush = CreateSolidBrush(kThemePanelBackground);
    FillRect(bufferDc, &clientRect, backgroundBrush);
    DeleteObject(backgroundBrush);

    const RECT trackRect = SliderTrackRect(clientRect);
    const int thumbCenterX = SliderThumbCenterX(clientRect, state);
    const int thumbCenterY = (clientRect.bottom - clientRect.top) / 2;
    const int activeWidth = std::max(0, thumbCenterX - static_cast<int>(trackRect.left));
    const RECT activeRect = MakeRect(trackRect.left, trackRect.top, activeWidth, static_cast<int>(trackRect.bottom - trackRect.top));

    const HBRUSH trackBrush = CreateSolidBrush(kThemeBorder);
    const HBRUSH activeBrush = CreateSolidBrush(kThemeAccent);
    const HBRUSH thumbBrush = CreateSolidBrush(kThemeText);
    const HPEN thumbPen = CreatePen(PS_SOLID, 2, kThemeAccent);
    const HGDIOBJ previousBrush = SelectObject(bufferDc, trackBrush);
    const HGDIOBJ previousPen = SelectObject(bufferDc, GetStockObject(NULL_PEN));
    RoundRect(bufferDc, trackRect.left, trackRect.top, trackRect.right, trackRect.bottom, kSliderTrackHeight, kSliderTrackHeight);
    if (activeRect.right > activeRect.left)
    {
        SelectObject(bufferDc, activeBrush);
        RoundRect(bufferDc, activeRect.left, activeRect.top, activeRect.right, activeRect.bottom, kSliderTrackHeight, kSliderTrackHeight);
    }
    SelectObject(bufferDc, thumbBrush);
    SelectObject(bufferDc, thumbPen);
    Ellipse(bufferDc, thumbCenterX - kSliderThumbRadius, thumbCenterY - kSliderThumbRadius, thumbCenterX + kSliderThumbRadius, thumbCenterY + kSliderThumbRadius);
    SelectObject(bufferDc, previousBrush);
    SelectObject(bufferDc, previousPen);
    DeleteObject(trackBrush);
    DeleteObject(activeBrush);
    DeleteObject(thumbBrush);
    DeleteObject(thumbPen);

    BitBlt(dc, 0, 0, std::max(1, width), std::max(1, height), bufferDc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, previousBitmap);
    DeleteObject(bufferBitmap);
    DeleteDC(bufferDc);
    EndPaint(hwnd, &paint);
}

void UpdateSliderValue(const HWND hwnd, SliderState& state, const int value, const bool notifyParent)
{
    const int clampedValue = ClampInt(value, state.minimum, state.maximum);
    if (state.value == clampedValue)
    {
        return;
    }

    state.value = clampedValue;
    InvalidateRect(hwnd, nullptr, FALSE);

    if (notifyParent)
    {
        SendMessageW(GetParent(hwnd), kSliderChangedMessage, reinterpret_cast<WPARAM>(hwnd), state.value);
    }
}

LRESULT CALLBACK SliderProc(const HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* state = new SliderState{};
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }

    auto* state = reinterpret_cast<SliderState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        PaintSlider(hwnd, *state);
        return 0;

    case WM_LBUTTONDOWN:
    {
        SetFocus(hwnd);
        SetCapture(hwnd);
        state->dragging = true;
        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);
        UpdateSliderValue(hwnd, *state, SliderValueFromPoint(clientRect, *state, GET_X_LPARAM(lParam)), true);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (state->dragging)
        {
            RECT clientRect{};
            GetClientRect(hwnd, &clientRect);
            UpdateSliderValue(hwnd, *state, SliderValueFromPoint(clientRect, *state, GET_X_LPARAM(lParam)), true);
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (state->dragging)
        {
            state->dragging = false;
            ReleaseCapture();
            RECT clientRect{};
            GetClientRect(hwnd, &clientRect);
            UpdateSliderValue(hwnd, *state, SliderValueFromPoint(clientRect, *state, GET_X_LPARAM(lParam)), true);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        state->dragging = false;
        return 0;

    case kSliderSetRangeMessage:
        state->minimum = static_cast<int>(wParam);
        state->maximum = std::max(state->minimum, static_cast<int>(lParam));
        state->value = ClampInt(state->value, state->minimum, state->maximum);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case kSliderSetValueMessage:
        UpdateSliderValue(hwnd, *state, static_cast<int>(wParam), false);
        return 0;

    case WM_NCDESTROY:
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

AppSection SectionFromId(const int id)
{
    switch (id)
    {
    case ID_SECTION_GENERAL:
        return AppSection::General;
    case ID_SECTION_CROSSHAIR:
        return AppSection::Crosshair;
    case ID_SECTION_CUSTOM_IMAGE:
        return AppSection::CustomImage;
    case ID_SECTION_HOTKEYS:
        return AppSection::Hotkeys;
    case ID_SECTION_ABOUT:
    default:
        return AppSection::About;
    }
}

const wchar_t* SectionTitle(const AppSection section)
{
    switch (section)
    {
    case AppSection::General:
        return L"General";
    case AppSection::Crosshair:
        return L"Crosshair";
    case AppSection::CustomImage:
        return L"Custom Image";
    case AppSection::Hotkeys:
        return L"Hotkeys";
    case AppSection::About:
    default:
        return L"About";
    }
}
} // namespace

MainWindow::MainWindow(AppController* app)
    : app_(app)
{
}

MainWindow::~MainWindow()
{
    if (titleFont_ != nullptr)
    {
        DeleteObject(titleFont_);
    }
    if (bodyFont_ != nullptr)
    {
        DeleteObject(bodyFont_);
    }
    if (smallFont_ != nullptr)
    {
        DeleteObject(smallFont_);
    }
    if (monoFont_ != nullptr)
    {
        DeleteObject(monoFont_);
    }
    if (windowBrush_ != nullptr)
    {
        DeleteObject(windowBrush_);
    }
    if (panelBrush_ != nullptr)
    {
        DeleteObject(panelBrush_);
    }
}

bool MainWindow::Create(const HINSTANCE instance)
{
    instance_ = instance;
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    CreateFonts();

    windowBrush_ = CreateSolidBrush(kThemeWindowBackground);
    panelBrush_ = CreateSolidBrush(kThemePanelBackground);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &MainWindow::WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kAppWindowClass;
    RegisterClassExW(&windowClass);

    WNDCLASSEXW previewClass{};
    previewClass.cbSize = sizeof(previewClass);
    previewClass.lpfnWndProc = &MainWindow::PreviewProc;
    previewClass.hInstance = instance_;
    previewClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    previewClass.hbrBackground = nullptr;
    previewClass.lpszClassName = kPreviewWindowClass;
    RegisterClassExW(&previewClass);

    WNDCLASSEXW sliderClass{};
    sliderClass.cbSize = sizeof(sliderClass);
    sliderClass.lpfnWndProc = &SliderProc;
    sliderClass.hInstance = instance_;
    sliderClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    sliderClass.hbrBackground = nullptr;
    sliderClass.lpszClassName = kSliderWindowClass;
    RegisterClassExW(&sliderClass);

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kAppWindowClass,
        kAppName,
        WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ == nullptr)
    {
        return false;
    }

    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON))));
    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON))));
    ApplyWindowFrame();
    UpdateWindowRegion(kWindowWidth, kWindowHeight);

    CreateControls();
    LayoutChildren(kWindowWidth, kWindowHeight);
    SetActiveSection(AppSection::General);
    return true;
}

void MainWindow::Destroy()
{
    if (hwnd_ != nullptr)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void MainWindow::Show(const int)
{
    CenterWindow();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
}

void MainWindow::ShowAndFocus()
{
    CenterWindow();
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}

void MainWindow::HideToTray()
{
    ShowWindow(hwnd_, SW_HIDE);
}

void MainWindow::ApplySettings(const Settings& settings)
{
    if (hwnd_ == nullptr)
    {
        return;
    }

    updatingControls_ = true;
    SetWindowTextW(styleButton_, StyleDisplayName(settings.style));
    SendMessageW(lengthSlider_, kSliderSetValueMessage, settings.length, 0);
    SendMessageW(gapSlider_, kSliderSetValueMessage, settings.gap, 0);
    SendMessageW(thicknessSlider_, kSliderSetValueMessage, settings.thickness, 0);
    SendMessageW(opacitySlider_, kSliderSetValueMessage, settings.opacity, 0);
    SendMessageW(rotationSlider_, kSliderSetValueMessage, settings.rotation, 0);
    SendMessageW(imageSizeSlider_, kSliderSetValueMessage, settings.imageSize, 0);

    UpdateSliderLabels(settings);
    UpdateImageStatus(settings);
    SetWindowTextW(generalStorageLabel_, (L"Settings path\r\n" + app_->settingsPath()).c_str());

    const bool hasImage = !settings.customImagePath.empty();
    EnableWindow(enableCustomImageCheck_, hasImage ? TRUE : FALSE);
    EnableWindow(clearImageButton_, hasImage ? TRUE : FALSE);
    EnableWindow(imageSizeSlider_, hasImage ? TRUE : FALSE);
    updatingControls_ = false;

    UpdateOwnerDrawControls();
    InvalidateRect(previewControl_, nullptr, TRUE);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

HWND MainWindow::hwnd() const
{
    return hwnd_;
}

LRESULT CALLBACK MainWindow::WindowProc(const HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* window = static_cast<MainWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
        return TRUE;
    }

    auto* window = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window != nullptr)
    {
        return window->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::PreviewProc(const HWND hwnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return TRUE;
    }

    auto* window = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window != nullptr && message == WM_PAINT)
    {
        window->PaintPreview(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    if (message == taskbarCreatedMessage_)
    {
        app_->RecreateTrayIcon();
        return 0;
    }

    switch (message)
    {
    case WM_COMMAND:
        HandleCommand(wParam, lParam);
        return 0;

    case kSliderChangedMessage:
        HandleSliderChanged(reinterpret_cast<HWND>(wParam), static_cast<int>(lParam));
        return 0;

    case WM_DRAWITEM:
        DrawOwnerButton(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kThemeText);
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    case WM_CTLCOLORLISTBOX:
    {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, kThemePanelSecondary);
        SetTextColor(dc, kThemeText);
        return reinterpret_cast<LRESULT>(panelBrush_);
    }

    case WM_PAINT:
        PaintWindow();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_NCCALCSIZE:
        return wParam != FALSE ? 0 : DefWindowProcW(hwnd_, message, wParam, lParam);

    case WM_NCPAINT:
        return 0;

    case WM_NCACTIVATE:
        return TRUE;

    case WM_GETMINMAXINFO:
    {
        auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
        minMaxInfo->ptMinTrackSize.x = kWindowMinWidth;
        minMaxInfo->ptMinTrackSize.y = kWindowMinHeight;
        return 0;
    }

    case WM_NCHITTEST:
    {
        POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT windowRect{};
        GetWindowRect(hwnd_, &windowRect);

        const bool left = screenPoint.x >= windowRect.left && screenPoint.x < windowRect.left + kResizeBorder;
        const bool right = screenPoint.x < windowRect.right && screenPoint.x >= windowRect.right - kResizeBorder;
        const bool top = screenPoint.y >= windowRect.top && screenPoint.y < windowRect.top + kResizeBorder;
        const bool bottom = screenPoint.y < windowRect.bottom && screenPoint.y >= windowRect.bottom - kResizeBorder;

        if (top && left)
        {
            return HTTOPLEFT;
        }
        if (top && right)
        {
            return HTTOPRIGHT;
        }
        if (bottom && left)
        {
            return HTBOTTOMLEFT;
        }
        if (bottom && right)
        {
            return HTBOTTOMRIGHT;
        }
        if (left)
        {
            return HTLEFT;
        }
        if (right)
        {
            return HTRIGHT;
        }
        if (top)
        {
            return HTTOP;
        }
        if (bottom)
        {
            return HTBOTTOM;
        }

        POINT clientPoint = screenPoint;
        ScreenToClient(hwnd_, &clientPoint);
        if (IsHeaderPoint(clientPoint))
        {
            return HTCAPTION;
        }

        return HTCLIENT;
    }

    case WM_LBUTTONDOWN:
    {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsHeaderPoint(point))
        {
            ReleaseCapture();
            SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        return 0;
    }

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            HideInsteadOfClose();
            return 0;
        }

        UpdateWindowRegion(LOWORD(lParam), HIWORD(lParam));
        LayoutChildren(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_CLOSE:
        HideInsteadOfClose();
        return 0;

    case kTrayCallbackMessage:
        HandleTrayMessage(lParam);
        return 0;

    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void MainWindow::CreateFonts()
{
    titleFont_ = CreateFontW(26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Semibold");
    bodyFont_ = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    smallFont_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    monoFont_ = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
}

void MainWindow::CreateControls()
{
    headerMinimizeButton_ = CreateWindowExW(0, L"BUTTON", L"-", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_HEADER_MINIMIZE), instance_, nullptr);
    headerCloseButton_ = CreateWindowExW(0, L"BUTTON", L"x", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_HEADER_CLOSE), instance_, nullptr);

    sectionButtons_[0] = CreateWindowExW(0, L"BUTTON", L"General", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_SECTION_GENERAL), instance_, nullptr);
    sectionButtons_[1] = CreateWindowExW(0, L"BUTTON", L"Crosshair", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_SECTION_CROSSHAIR), instance_, nullptr);
    sectionButtons_[2] = CreateWindowExW(0, L"BUTTON", L"Custom Image", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_SECTION_CUSTOM_IMAGE), instance_, nullptr);
    sectionButtons_[3] = CreateWindowExW(0, L"BUTTON", L"Hotkeys", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_SECTION_HOTKEYS), instance_, nullptr);
    sectionButtons_[4] = CreateWindowExW(0, L"BUTTON", L"About", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_SECTION_ABOUT), instance_, nullptr);

    generalVisibleCheck_ = CreateWindowExW(0, L"BUTTON", L"Overlay enabled", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_GENERAL_VISIBLE), instance_, nullptr);
    generalSummaryLabel_ = CreateWindowExW(0, L"STATIC", L"The settings window opens when the app starts.\r\nMinimize and close both send it to the tray.\r\nLeft-click the tray icon to open it again.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    generalStorageLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    previewControl_ = CreateWindowExW(0, kPreviewWindowClass, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_PREVIEW_CONTROL), instance_, this);
    styleButton_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_STYLE_COMBO), instance_, nullptr);

    constexpr DWORD kSliderStyle = WS_CHILD | WS_VISIBLE;
    lengthSlider_ = CreateWindowExW(0, kSliderWindowClass, nullptr, kSliderStyle, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_LENGTH_SLIDER), instance_, nullptr);
    gapSlider_ = CreateWindowExW(0, kSliderWindowClass, nullptr, kSliderStyle, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_GAP_SLIDER), instance_, nullptr);
    thicknessSlider_ = CreateWindowExW(0, kSliderWindowClass, nullptr, kSliderStyle, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_THICKNESS_SLIDER), instance_, nullptr);
    opacitySlider_ = CreateWindowExW(0, kSliderWindowClass, nullptr, kSliderStyle, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_OPACITY_SLIDER), instance_, nullptr);
    rotationSlider_ = CreateWindowExW(0, kSliderWindowClass, nullptr, kSliderStyle, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_ROTATION_SLIDER), instance_, nullptr);
    SendMessageW(lengthSlider_, kSliderSetRangeMessage, 4, 120);
    SendMessageW(gapSlider_, kSliderSetRangeMessage, 0, 80);
    SendMessageW(thicknessSlider_, kSliderSetRangeMessage, 1, 20);
    SendMessageW(opacitySlider_, kSliderSetRangeMessage, 40, 255);
    SendMessageW(rotationSlider_, kSliderSetRangeMessage, 0, 359);

    lengthValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    gapValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    thicknessValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    opacityValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    rotationValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    for (size_t index = 0; index < swatchButtons_.size(); ++index)
    {
        swatchButtons_[index] = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SWATCH_BASE + static_cast<int>(index))), instance_, nullptr);
    }

    useCustomColorCheck_ = CreateWindowExW(0, L"BUTTON", L"Use custom color", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_USE_CUSTOM_COLOR), instance_, nullptr);
    pickColorButton_ = CreateWindowExW(0, L"BUTTON", L"Choose Color", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_PICK_COLOR_BUTTON), instance_, nullptr);
    customColorSwatch_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_CUSTOM_COLOR_SWATCH), instance_, nullptr);
    middleDotCheck_ = CreateWindowExW(0, L"BUTTON", L"Middle dot", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_MIDDLE_DOT), instance_, nullptr);
    outlineCheck_ = CreateWindowExW(0, L"BUTTON", L"Outline", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_OUTLINE_ENABLED), instance_, nullptr);

    enableCustomImageCheck_ = CreateWindowExW(0, L"BUTTON", L"Use PNG crosshair", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_ENABLE_CUSTOM_IMAGE), instance_, nullptr);
    importImageButton_ = CreateWindowExW(0, L"BUTTON", L"Import PNG", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_IMPORT_IMAGE), instance_, nullptr);
    clearImageButton_ = CreateWindowExW(0, L"BUTTON", L"Clear PNG", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_CLEAR_IMAGE), instance_, nullptr);
    imageStatusLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    imageSizeSlider_ = CreateWindowExW(0, kSliderWindowClass, nullptr, kSliderStyle, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_IMAGE_SIZE_SLIDER), instance_, nullptr);
    SendMessageW(imageSizeSlider_, kSliderSetRangeMessage, 16, 512);
    imageSizeValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    hotkeysText_ = CreateWindowExW(0, L"STATIC", L"F8  Toggle overlay\r\nF9  Cycle built-in style\r\nCtrl + Alt + C  Cycle preset color\r\nCtrl + Alt + Up / Down  Length\r\nCtrl + Alt + Left / Right  Gap\r\nCtrl + Alt + PageUp / PageDown  Thickness\r\nCtrl + Alt + Home / End  Opacity\r\nCtrl + Alt + R  Reset defaults\r\nCtrl + Alt + Q  Quit", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    aboutText_ = CreateWindowExW(0, L"STATIC", L"Potato Crosshair\r\nA simple crosshair overlay for Windows.\r\nIt has built-in crosshairs, custom colors, PNG support, and a tray icon.\r\n\r\nIt still uses the main monitor only.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    for (const HWND control : sectionButtons_)
    {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
    }

    const std::array<HWND, 20> bodyControls = {
        headerMinimizeButton_, headerCloseButton_, generalVisibleCheck_, generalSummaryLabel_, generalStorageLabel_, previewControl_,
        styleButton_, lengthValue_, gapValue_, thicknessValue_, opacityValue_, rotationValue_, useCustomColorCheck_,
        pickColorButton_, customColorSwatch_, middleDotCheck_, outlineCheck_, enableCustomImageCheck_, imageStatusLabel_, imageSizeValue_};

    for (const HWND control : bodyControls)
    {
        if (control != nullptr)
        {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
        }
    }

    SendMessageW(imageSizeSlider_, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
    SendMessageW(hotkeysText_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    SendMessageW(aboutText_, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);

    RegisterSectionControl(generalVisibleCheck_, AppSection::General);
    RegisterSectionControl(generalSummaryLabel_, AppSection::General);
    RegisterSectionControl(generalStorageLabel_, AppSection::General);
    RegisterSectionControl(previewControl_, AppSection::Crosshair);
    RegisterSectionControl(styleButton_, AppSection::Crosshair);
    RegisterSectionControl(lengthSlider_, AppSection::Crosshair);
    RegisterSectionControl(lengthValue_, AppSection::Crosshair);
    RegisterSectionControl(gapSlider_, AppSection::Crosshair);
    RegisterSectionControl(gapValue_, AppSection::Crosshair);
    RegisterSectionControl(thicknessSlider_, AppSection::Crosshair);
    RegisterSectionControl(thicknessValue_, AppSection::Crosshair);
    RegisterSectionControl(opacitySlider_, AppSection::Crosshair);
    RegisterSectionControl(opacityValue_, AppSection::Crosshair);
    RegisterSectionControl(rotationSlider_, AppSection::Crosshair);
    RegisterSectionControl(rotationValue_, AppSection::Crosshair);
    RegisterSectionControl(useCustomColorCheck_, AppSection::Crosshair);
    RegisterSectionControl(pickColorButton_, AppSection::Crosshair);
    RegisterSectionControl(customColorSwatch_, AppSection::Crosshair);
    RegisterSectionControl(middleDotCheck_, AppSection::Crosshair);
    RegisterSectionControl(outlineCheck_, AppSection::Crosshair);
    for (const HWND swatch : swatchButtons_)
    {
        RegisterSectionControl(swatch, AppSection::Crosshair);
    }
    RegisterSectionControl(enableCustomImageCheck_, AppSection::CustomImage);
    RegisterSectionControl(importImageButton_, AppSection::CustomImage);
    RegisterSectionControl(clearImageButton_, AppSection::CustomImage);
    RegisterSectionControl(imageStatusLabel_, AppSection::CustomImage);
    RegisterSectionControl(imageSizeSlider_, AppSection::CustomImage);
    RegisterSectionControl(imageSizeValue_, AppSection::CustomImage);
    RegisterSectionControl(hotkeysText_, AppSection::Hotkeys);
    RegisterSectionControl(aboutText_, AppSection::About);
}

void MainWindow::LayoutChildren(const int width, const int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    constexpr UINT kLayoutFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOREDRAW;
    const int sidebarTop = kHeaderHeight + 24;
    const int contentLeft = kSidebarWidth + 30;
    const int contentWidth = width - contentLeft - 24;
    const int cardLeft = contentLeft + 12;
    const int contentTop = 130;
    const int sectionHeight = std::max(420, height - contentTop - 24);
    const int availableWidth = contentWidth - 24;
    HDWP deferred = BeginDeferWindowPos(48);

    auto placeControl = [&](const HWND control, const int x, const int y, const int controlWidth, const int controlHeight)
    {
        if (control == nullptr)
        {
            return;
        }

        if (deferred != nullptr)
        {
            deferred = DeferWindowPos(deferred, control, nullptr, x, y, controlWidth, controlHeight, kLayoutFlags);
        }
        else
        {
            SetWindowPos(control, nullptr, x, y, controlWidth, controlHeight, kLayoutFlags);
        }
    };

    placeControl(headerMinimizeButton_, width - 104, 20, 36, 32);
    placeControl(headerCloseButton_, width - 60, 20, 36, 32);

    int sidebarY = sidebarTop;
    for (const HWND button : sectionButtons_)
    {
        placeControl(button, 18, sidebarY, kSidebarWidth - 36, kSidebarButtonHeight);
        sidebarY += kSidebarButtonHeight + kSidebarGap;
    }

    placeControl(generalVisibleCheck_, cardLeft + 24, contentTop + 28, availableWidth - 48, 34);
    placeControl(generalSummaryLabel_, cardLeft + 24, contentTop + 82, availableWidth - 48, 96);
    placeControl(generalStorageLabel_, cardLeft + 24, contentTop + 202, availableWidth - 48, 80);

    const CrosshairSectionLayout crosshairLayout = CalculateCrosshairSectionLayout(cardLeft, contentTop, availableWidth, sectionHeight);

    placeControl(previewControl_, crosshairLayout.previewRect.left, crosshairLayout.previewRect.top, RectWidth(crosshairLayout.previewRect), RectHeight(crosshairLayout.previewRect));

    const int controlsInnerLeft = crosshairLayout.controlsCard.left + kCardPadding;
    const int controlsInnerWidth = RectWidth(crosshairLayout.controlsCard) - (kCardPadding * 2);
    const int controlsBottom = crosshairLayout.controlsCard.bottom - kCardPadding;
    int y = crosshairLayout.controlsCard.top + kCardTitleHeight;
    placeControl(styleButton_, controlsInnerLeft, y, controlsInnerWidth, kStyleButtonHeight);
    y += kStyleButtonHeight + 10;

    const int sliderHeight = 22;
    const int labelHeight = 20;
    const int groupGap = 6;
    placeControl(lengthValue_, controlsInnerLeft, y, controlsInnerWidth, labelHeight);
    y += labelHeight;
    placeControl(lengthSlider_, controlsInnerLeft, y, controlsInnerWidth, sliderHeight);
    y += sliderHeight + groupGap;
    placeControl(gapValue_, controlsInnerLeft, y, controlsInnerWidth, labelHeight);
    y += labelHeight;
    placeControl(gapSlider_, controlsInnerLeft, y, controlsInnerWidth, sliderHeight);
    y += sliderHeight + groupGap;
    placeControl(thicknessValue_, controlsInnerLeft, y, controlsInnerWidth, labelHeight);
    y += labelHeight;
    placeControl(thicknessSlider_, controlsInnerLeft, y, controlsInnerWidth, sliderHeight);
    y += sliderHeight + groupGap;
    placeControl(opacityValue_, controlsInnerLeft, y, controlsInnerWidth, labelHeight);
    y += labelHeight;
    placeControl(opacitySlider_, controlsInnerLeft, y, controlsInnerWidth, sliderHeight);
    y += sliderHeight + groupGap;
    placeControl(rotationValue_, controlsInnerLeft, y, controlsInnerWidth, labelHeight);
    y += labelHeight;
    placeControl(rotationSlider_, controlsInnerLeft, y, controlsInnerWidth, sliderHeight);
    y += sliderHeight + 10;

    int swatchLeft = controlsInnerLeft;
    for (const HWND swatch : swatchButtons_)
    {
        placeControl(swatch, swatchLeft, y, 26, 26);
        swatchLeft += 36;
    }
    y += 34;

    placeControl(useCustomColorCheck_, controlsInnerLeft, y, controlsInnerWidth, kToggleHeight);
    y += kToggleHeight + 8;
    const int pickColorButtonWidth = std::max(130, std::min(176, controlsInnerWidth - 50));
    placeControl(pickColorButton_, controlsInnerLeft, y, pickColorButtonWidth, 36);
    placeControl(customColorSwatch_, controlsInnerLeft + pickColorButtonWidth + 10, y, 36, 36);
    y += 46;

    const int toggleSpacing = 12;
    const int toggleWidth = std::max(120, (controlsInnerWidth - toggleSpacing) / 2);
    const int toggleTop = std::min(y, controlsBottom - kToggleHeight);
    placeControl(middleDotCheck_, controlsInnerLeft, toggleTop, toggleWidth, kToggleHeight);
    placeControl(outlineCheck_, controlsInnerLeft + toggleWidth + toggleSpacing, toggleTop, toggleWidth, kToggleHeight);

    placeControl(enableCustomImageCheck_, cardLeft + 24, contentTop + 28, availableWidth - 48, kToggleHeight);
    const int imageButtonWidth = std::max(128, std::min(148, (availableWidth - 60) / 2));
    placeControl(importImageButton_, cardLeft + 24, contentTop + 82, imageButtonWidth, 38);
    placeControl(clearImageButton_, cardLeft + 36 + imageButtonWidth, contentTop + 82, imageButtonWidth, 38);
    placeControl(imageStatusLabel_, cardLeft + 24, contentTop + 144, availableWidth - 48, 40);
    placeControl(imageSizeValue_, cardLeft + 24, contentTop + 206, availableWidth - 48, 22);
    placeControl(imageSizeSlider_, cardLeft + 24, contentTop + 232, availableWidth - 48, 24);

    placeControl(hotkeysText_, cardLeft + 28, contentTop + 28, availableWidth - 56, sectionHeight - 56);
    placeControl(aboutText_, cardLeft + 28, contentTop + 28, availableWidth - 56, sectionHeight - 56);

    if (deferred != nullptr)
    {
        EndDeferWindowPos(deferred);
    }

    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
}

void MainWindow::RegisterSectionControl(const HWND handle, const AppSection section)
{
    sectionControls_.push_back({handle, section});
}

void MainWindow::SetActiveSection(const AppSection section)
{
    activeSection_ = section;
    UpdateSectionVisibility();
    UpdateOwnerDrawControls();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainWindow::UpdateSectionVisibility()
{
    for (const auto& entry : sectionControls_)
    {
        ShowWindow(entry.handle, entry.section == activeSection_ ? SW_SHOW : SW_HIDE);
    }
}

void MainWindow::UpdateSliderLabels(const Settings& settings)
{
    SetWindowTextW(lengthValue_, SliderText(L"Length ", settings.length).c_str());
    SetWindowTextW(gapValue_, SliderText(L"Gap ", settings.gap).c_str());
    SetWindowTextW(thicknessValue_, SliderText(L"Thickness ", settings.thickness).c_str());
    SetWindowTextW(opacityValue_, SliderText(L"Opacity ", settings.opacity).c_str());
    SetWindowTextW(rotationValue_, SliderText(L"Rotation ", settings.rotation, L"\x00B0").c_str());
    SetWindowTextW(imageSizeValue_, SliderText(L"PNG size ", settings.imageSize, L" px").c_str());
}

void MainWindow::UpdateImageStatus(const Settings& settings)
{
    SetWindowTextW(imageStatusLabel_, FileLabelText(settings).c_str());
}

void MainWindow::UpdateOwnerDrawControls()
{
    for (const HWND button : sectionButtons_)
    {
        InvalidateRect(button, nullptr, TRUE);
    }
    for (const HWND swatch : swatchButtons_)
    {
        InvalidateRect(swatch, nullptr, TRUE);
    }
    InvalidateRect(generalVisibleCheck_, nullptr, TRUE);
    InvalidateRect(styleButton_, nullptr, TRUE);
    InvalidateRect(useCustomColorCheck_, nullptr, TRUE);
    InvalidateRect(pickColorButton_, nullptr, TRUE);
    InvalidateRect(customColorSwatch_, nullptr, TRUE);
    InvalidateRect(middleDotCheck_, nullptr, TRUE);
    InvalidateRect(outlineCheck_, nullptr, TRUE);
    InvalidateRect(enableCustomImageCheck_, nullptr, TRUE);
    InvalidateRect(importImageButton_, nullptr, TRUE);
    InvalidateRect(clearImageButton_, nullptr, TRUE);
    InvalidateRect(headerMinimizeButton_, nullptr, TRUE);
    InvalidateRect(headerCloseButton_, nullptr, TRUE);
}

void MainWindow::PaintWindow()
{
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(hwnd_, &paint);

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0)
    {
        EndPaint(hwnd_, &paint);
        return;
    }

    const HDC bufferDc = CreateCompatibleDC(dc);
    const HBITMAP bufferBitmap = CreateCompatibleBitmap(dc, clientWidth, clientHeight);
    const HGDIOBJ previousBitmap = SelectObject(bufferDc, bufferBitmap);
    FillRect(bufferDc, &clientRect, windowBrush_);

    const RECT headerRect = MakeRect(0, 0, clientRect.right, kHeaderHeight);
    const RECT sidebarRect = MakeRect(0, kHeaderHeight, kSidebarWidth, clientRect.bottom - kHeaderHeight);
    const RECT contentRect = MakeRect(kSidebarWidth + 6, kHeaderHeight + 8, clientRect.right - kSidebarWidth - 18, clientRect.bottom - kHeaderHeight - 16);
    const int cardLeft = contentRect.left + 12;
    const int sectionTop = 130;
    const int sectionHeight = std::max(420, static_cast<int>(clientRect.bottom) - sectionTop - 24);
    const int availableWidth = (contentRect.right - contentRect.left) - 24;

    HBRUSH headerBrush = CreateSolidBrush(kThemePanelSecondary);
    FillRect(bufferDc, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    HBRUSH sidebarBrush = CreateSolidBrush(kThemePanelBackground);
    FillRect(bufferDc, &sidebarRect, sidebarBrush);
    DeleteObject(sidebarBrush);

    SetBkMode(bufferDc, TRANSPARENT);
    SetTextColor(bufferDc, kThemeText);
    SelectObject(bufferDc, titleFont_);
    TextOutW(bufferDc, 28, 22, L"Potato Crosshair", 17);
    SelectObject(bufferDc, smallFont_);
    SetTextColor(bufferDc, kThemeMutedText);
    TextOutW(bufferDc, 30, 50, L"Settings", 8);
    SelectObject(bufferDc, bodyFont_);
    SetTextColor(bufferDc, kThemeText);
    TextOutW(bufferDc, 24, kHeaderHeight + 10, L"Sections", 8);

    SetTextColor(bufferDc, kThemeAccentHover);
    const wchar_t* currentSection = SectionTitle(activeSection_);
    TextOutW(bufferDc, contentRect.left + 18, kHeaderHeight + 18, currentSection, static_cast<int>(wcslen(currentSection)));

    if (activeSection_ == AppSection::General)
    {
        DrawCard(bufferDc, MakeRect(cardLeft, sectionTop, availableWidth, sectionHeight));
    }
    else if (activeSection_ == AppSection::Crosshair)
    {
        const CrosshairSectionLayout crosshairLayout = CalculateCrosshairSectionLayout(cardLeft, sectionTop, availableWidth, sectionHeight);

        DrawCard(bufferDc, crosshairLayout.previewCard);
        DrawCard(bufferDc, crosshairLayout.controlsCard);

        SetTextColor(bufferDc, kThemeText);
        SelectObject(bufferDc, bodyFont_);
        TextOutW(bufferDc, crosshairLayout.previewCard.left + 20, crosshairLayout.previewCard.top + 18, L"Live preview", 12);
        TextOutW(bufferDc, crosshairLayout.controlsCard.left + 20, crosshairLayout.controlsCard.top + 18, L"Style settings", 14);

        SelectObject(bufferDc, smallFont_);
        SetTextColor(bufferDc, kThemeMutedText);
        TextOutW(bufferDc, crosshairLayout.controlsCard.left + 20, crosshairLayout.controlsCard.top + 40, L"Each built-in style keeps its own settings.", 41);
    }
    else
    {
        DrawCard(bufferDc, MakeRect(cardLeft, sectionTop, availableWidth, sectionHeight));
    }

    BitBlt(dc, 0, 0, clientWidth, clientHeight, bufferDc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, previousBitmap);
    DeleteObject(bufferBitmap);
    DeleteDC(bufferDc);
    EndPaint(hwnd_, &paint);
}

void MainWindow::PaintPreview(const HWND preview) const
{
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(preview, &paint);

    RECT clientRect{};
    GetClientRect(preview, &clientRect);
    FillRoundRect(dc, clientRect, 18, kThemePanelSecondary, kThemeBorder);

    const int centerX = clientRect.left + (RectWidth(clientRect) / 2);
    const int centerY = clientRect.top + (RectHeight(clientRect) / 2);
    const HPEN guidePen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
    const HGDIOBJ previousPen = SelectObject(dc, guidePen);
    MoveToEx(dc, clientRect.left + 24, centerY, nullptr);
    LineTo(dc, clientRect.right - 24, centerY);
    MoveToEx(dc, centerX, clientRect.top + 24, nullptr);
    LineTo(dc, centerX, clientRect.bottom - 24);
    SelectObject(dc, previousPen);
    DeleteObject(guidePen);

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const RECT inner = InflateRectCopy(clientRect, -18);
    DrawCrosshair(graphics, app_->settings(), Gdiplus::RectF(static_cast<float>(inner.left), static_cast<float>(inner.top), static_cast<float>(inner.right - inner.left), static_cast<float>(inner.bottom - inner.top)));

    EndPaint(preview, &paint);
}

void MainWindow::DrawOwnerButton(const DRAWITEMSTRUCT& drawItem) const
{
    const int controlId = static_cast<int>(drawItem.CtlID);
    const bool selected = (drawItem.itemState & ODS_SELECTED) != 0;
    const bool disabled = (drawItem.itemState & ODS_DISABLED) != 0;
    const Settings& settings = app_->settings();

    COLORREF fillColor = kThemePanelSecondary;
    COLORREF borderColor = kThemeBorder;
    COLORREF textColor = disabled ? kThemeMutedText : kThemeText;

    if (IsToggleControlId(controlId))
    {
        fillColor = selected ? kThemePanelSecondary : kThemePanelBackground;
        borderColor = selected ? kThemeAccent : kThemeBorder;
        FillRoundRect(drawItem.hDC, drawItem.rcItem, 12, fillColor, borderColor);

        wchar_t text[128]{};
        GetWindowTextW(drawItem.hwndItem, text, static_cast<int>(std::size(text)));

        RECT textRect = drawItem.rcItem;
        textRect.left += 14;
        textRect.right -= kToggleTrackWidth + 26;
        SetBkMode(drawItem.hDC, TRANSPARENT);
        SetTextColor(drawItem.hDC, textColor);
        SelectObject(drawItem.hDC, bodyFont_);
        DrawTextW(drawItem.hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT toggleRect = drawItem.rcItem;
        toggleRect.right -= 12;
        toggleRect.left = toggleRect.right - kToggleTrackWidth;
        toggleRect.top += (drawItem.rcItem.bottom - drawItem.rcItem.top - kToggleTrackHeight) / 2;
        toggleRect.bottom = toggleRect.top + kToggleTrackHeight;

        const bool checked = ToggleStateForControl(settings, controlId);
        const COLORREF toggleFill = disabled ? kThemeBorder : (checked ? kThemeAccent : RGB(58, 58, 58));
        const COLORREF toggleBorder = checked ? kThemeAccentHover : kThemeBorder;
        FillRoundRect(drawItem.hDC, toggleRect, kToggleTrackHeight, toggleFill, toggleBorder);

        RECT thumbRect = toggleRect;
        thumbRect.top += kToggleThumbInset;
        thumbRect.bottom -= kToggleThumbInset;
        const int thumbSize = thumbRect.bottom - thumbRect.top;
        thumbRect.left = checked ? toggleRect.right - kToggleThumbInset - thumbSize : toggleRect.left + kToggleThumbInset;
        thumbRect.right = thumbRect.left + thumbSize;
        FillRoundRect(drawItem.hDC, thumbRect, thumbSize, kThemeText, checked ? kThemeAccentHover : kThemeBorder);
        return;
    }

    if (controlId == ID_STYLE_COMBO)
    {
        fillColor = selected ? kThemePanelSecondary : kThemePanelBackground;
        borderColor = selected ? kThemeAccent : kThemeBorder;
        FillRoundRect(drawItem.hDC, drawItem.rcItem, 12, fillColor, borderColor);

        RECT textRect = drawItem.rcItem;
        textRect.left += 14;
        textRect.right -= 36;
        SetBkMode(drawItem.hDC, TRANSPARENT);
        SetTextColor(drawItem.hDC, textColor);
        SelectObject(drawItem.hDC, bodyFont_);
        DrawTextW(drawItem.hDC, StyleDisplayName(settings.style), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const int centerX = drawItem.rcItem.right - 18;
        const int centerY = (drawItem.rcItem.top + drawItem.rcItem.bottom) / 2;
        const HPEN chevronPen = CreatePen(PS_SOLID, 2, textColor);
        const HGDIOBJ previousPen = SelectObject(drawItem.hDC, chevronPen);
        MoveToEx(drawItem.hDC, centerX - 5, centerY - 2, nullptr);
        LineTo(drawItem.hDC, centerX, centerY + 3);
        LineTo(drawItem.hDC, centerX + 5, centerY - 2);
        SelectObject(drawItem.hDC, previousPen);
        DeleteObject(chevronPen);
        return;
    }

    if (controlId >= ID_SECTION_GENERAL && controlId <= ID_SECTION_ABOUT)
    {
        fillColor = SectionFromId(controlId) == activeSection_ ? kThemeAccent : kThemePanelSecondary;
        borderColor = fillColor;
        textColor = SectionFromId(controlId) == activeSection_ ? RGB(20, 20, 20) : kThemeText;
    }
    else if (controlId == ID_HEADER_CLOSE)
    {
        fillColor = selected ? RGB(120, 48, 44) : kThemeDanger;
        borderColor = fillColor;
    }
    else if (controlId == ID_HEADER_MINIMIZE)
    {
        fillColor = selected ? kThemeAccentHover : kThemePanelSecondary;
    }
    else if (controlId == ID_PICK_COLOR_BUTTON || controlId == ID_IMPORT_IMAGE)
    {
        fillColor = selected ? kThemeAccentHover : kThemeAccent;
        borderColor = fillColor;
        textColor = RGB(20, 20, 20);
    }
    else if (controlId == ID_CLEAR_IMAGE)
    {
        fillColor = selected ? RGB(130, 62, 55) : kThemeDanger;
        borderColor = fillColor;
    }
    else if (controlId == ID_CUSTOM_COLOR_SWATCH)
    {
        fillColor = settings.customColor;
        borderColor = settings.useCustomColor ? kThemeAccentHover : kThemeBorder;
    }
    else if (controlId >= ID_SWATCH_BASE && controlId < ID_SWATCH_BASE + static_cast<int>(kColorPresets.size()))
    {
        fillColor = GetPresetColor(controlId - ID_SWATCH_BASE);
        borderColor = (!settings.useCustomColor && settings.colorIndex == controlId - ID_SWATCH_BASE) ? kThemeAccentHover : kThemeBorder;
    }

    FillRoundRect(drawItem.hDC, drawItem.rcItem, 10, fillColor, borderColor);

    if (controlId == ID_CUSTOM_COLOR_SWATCH || (controlId >= ID_SWATCH_BASE && controlId < ID_SWATCH_BASE + static_cast<int>(kColorPresets.size())))
    {
        return;
    }

    wchar_t text[128]{};
    GetWindowTextW(drawItem.hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(drawItem.hDC, TRANSPARENT);
    SetTextColor(drawItem.hDC, textColor);
    SelectObject(drawItem.hDC, bodyFont_);
    RECT textRect = drawItem.rcItem;
    DrawTextW(drawItem.hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::HandleCommand(const WPARAM wParam, const LPARAM)
{
    const int controlId = LOWORD(wParam);
    const int notification = HIWORD(wParam);

    if (controlId >= ID_SECTION_GENERAL && controlId <= ID_SECTION_ABOUT && notification == BN_CLICKED)
    {
        SetActiveSection(SectionFromId(controlId));
        return;
    }
    if (controlId == ID_HEADER_MINIMIZE && notification == BN_CLICKED)
    {
        HideInsteadOfClose();
        return;
    }
    if (controlId == ID_HEADER_CLOSE && notification == BN_CLICKED)
    {
        HideInsteadOfClose();
        return;
    }
    if (controlId == ID_STYLE_COMBO && notification == BN_CLICKED)
    {
        OpenStyleMenu();
        return;
    }
    if (controlId == ID_TRAY_OPEN)
    {
        app_->ShowSettingsWindow();
        return;
    }
    if (controlId == ID_TRAY_TOGGLE)
    {
        app_->ToggleOverlayVisibility();
        return;
    }
    if (controlId == ID_TRAY_QUIT)
    {
        app_->RequestQuit();
        return;
    }
    if (updatingControls_)
    {
        return;
    }

    Settings updated = CopySettings();

    if (controlId == ID_GENERAL_VISIBLE && notification == BN_CLICKED)
    {
        updated.visible = !updated.visible;
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_USE_CUSTOM_COLOR && notification == BN_CLICKED)
    {
        updated.useCustomColor = !updated.useCustomColor;
        app_->ApplySettings(updated);
        return;
    }
    if ((controlId == ID_PICK_COLOR_BUTTON || controlId == ID_CUSTOM_COLOR_SWATCH) && notification == BN_CLICKED)
    {
        OpenColorDialog();
        return;
    }
    if (controlId >= ID_SWATCH_BASE && controlId < ID_SWATCH_BASE + static_cast<int>(kColorPresets.size()) && notification == BN_CLICKED)
    {
        updated.colorIndex = controlId - ID_SWATCH_BASE;
        updated.useCustomColor = false;
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_ENABLE_CUSTOM_IMAGE && notification == BN_CLICKED)
    {
        updated.renderMode = updated.renderMode == RenderMode::CustomImage ? RenderMode::BuiltIn : (!updated.customImagePath.empty() ? RenderMode::CustomImage : RenderMode::BuiltIn);
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_MIDDLE_DOT && notification == BN_CLICKED)
    {
        updated.middleDot = !updated.middleDot;
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_OUTLINE_ENABLED && notification == BN_CLICKED)
    {
        updated.outlineEnabled = !updated.outlineEnabled;
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_IMPORT_IMAGE && notification == BN_CLICKED)
    {
        OpenImportDialog();
        return;
    }
    if (controlId == ID_CLEAR_IMAGE && notification == BN_CLICKED)
    {
        app_->ClearCustomImage();
    }
}

void MainWindow::HandleSliderChanged(const HWND slider, const int value)
{
    if (updatingControls_)
    {
        return;
    }

    Settings updated = CopySettings();

    if (slider == lengthSlider_)
    {
        updated.length = value;
    }
    else if (slider == gapSlider_)
    {
        updated.gap = value;
    }
    else if (slider == thicknessSlider_)
    {
        updated.thickness = value;
    }
    else if (slider == opacitySlider_)
    {
        updated.opacity = value;
    }
    else if (slider == rotationSlider_)
    {
        updated.rotation = value;
    }
    else if (slider == imageSizeSlider_)
    {
        updated.imageSize = value;
    }
    else
    {
        return;
    }

    app_->ApplySettings(updated);
}

void MainWindow::HandleTrayMessage(const LPARAM lParam)
{
    switch (lParam)
    {
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        app_->ShowSettingsWindow();
        break;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        app_->tray().ShowContextMenu(app_->settings().visible);
        break;
    default:
        break;
    }
}

void MainWindow::OpenStyleMenu()
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    for (int index = 0; index < kCrosshairStyleCount; ++index)
    {
        const UINT flags = MF_STRING | (StyleToInt(app_->settings().style) == index ? MF_CHECKED : 0U);
        AppendMenuW(menu, flags, ID_STYLE_OPTION_BASE + index, StyleDisplayName(IntToStyle(index)));
    }

    RECT buttonRect{};
    GetWindowRect(styleButton_, &buttonRect);
    SetForegroundWindow(hwnd_);
    const int command = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_TOPALIGN | TPM_NONOTIFY, buttonRect.left, buttonRect.bottom + 4, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    if (command >= ID_STYLE_OPTION_BASE && command < ID_STYLE_OPTION_BASE + kCrosshairStyleCount)
    {
        Settings updated = CopySettings();
        SwitchStyleProfile(updated, IntToStyle(command - ID_STYLE_OPTION_BASE));
        app_->ApplySettings(updated);
    }
}

void MainWindow::OpenColorDialog()
{
    Settings updated = app_->settings();
    static COLORREF customColors[16] = {};

    CHOOSECOLORW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.rgbResult = updated.customColor;
    dialog.lpCustColors = customColors;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&dialog))
    {
        updated.customColor = dialog.rgbResult;
        updated.useCustomColor = true;
        app_->ApplySettings(updated);
    }
}

void MainWindow::OpenImportDialog()
{
    const std::wstring selectedPath = OpenPngFileDialog();
    if (!selectedPath.empty())
    {
        app_->ImportCustomImage(selectedPath);
    }
}

std::wstring MainWindow::OpenPngFileDialog() const
{
    std::array<wchar_t, MAX_PATH> filePath{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = L"PNG Files\0*.png\0All Files\0*.*\0\0";
    dialog.lpstrFile = filePath.data();
    dialog.nMaxFile = static_cast<DWORD>(filePath.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&dialog))
    {
        return filePath.data();
    }

    return {};
}

Settings MainWindow::CopySettings() const
{
    return app_->settings();
}

void MainWindow::ApplyWindowFrame()
{
    if (hwnd_ == nullptr)
    {
        return;
    }

    const BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    const auto cornerPreference = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(DWMWCP_ROUND);
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void MainWindow::UpdateWindowRegion(const int width, const int height)
{
    if (hwnd_ == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    if (IsZoomed(hwnd_) != FALSE)
    {
        SetWindowRgn(hwnd_, nullptr, TRUE);
        return;
    }

    const HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, kWindowCornerRadius, kWindowCornerRadius);
    SetWindowRgn(hwnd_, region, TRUE);
}

void MainWindow::CenterWindow()
{
    if (hwnd_ == nullptr || hasInitialPlacement_)
    {
        return;
    }

    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY);
    GetMonitorInfoW(monitor, &monitorInfo);

    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    const int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
    const int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
    const int x = monitorInfo.rcWork.left + (workWidth - width) / 2;
    const int y = monitorInfo.rcWork.top + (workHeight - height) / 2;

    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    hasInitialPlacement_ = true;
}

bool MainWindow::IsHeaderPoint(const POINT point) const
{
    if (point.y < 0 || point.y > kHeaderHeight)
    {
        return false;
    }

    return !IsPointInControl(headerMinimizeButton_, point) && !IsPointInControl(headerCloseButton_, point);
}

bool MainWindow::IsPointInControl(const HWND control, POINT point) const
{
    RECT rect{};
    GetWindowRect(control, &rect);
    POINT topLeft{rect.left, rect.top};
    POINT bottomRight{rect.right, rect.bottom};
    ScreenToClient(hwnd_, &topLeft);
    ScreenToClient(hwnd_, &bottomRight);
    rect.left = topLeft.x;
    rect.top = topLeft.y;
    rect.right = bottomRight.x;
    rect.bottom = bottomRight.y;
    return PtInRect(&rect, point) != FALSE;
}

void MainWindow::HideInsteadOfClose()
{
    if (!app_->isQuitting())
    {
        HideToTray();
    }
}
} // namespace potato
