#include "main_window.h"

#include "app_controller.h"
#include "crosshair_renderer.h"
#include "resource.h"
#include "tray_icon.h"

#include <commctrl.h>
#include <commdlg.h>
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
constexpr int kHeaderHeight = 78;
constexpr int kSidebarWidth = 190;
constexpr int kSidebarButtonHeight = 42;
constexpr int kSidebarGap = 10;

RECT MakeRect(const int left, const int top, const int width, const int height)
{
    RECT rect{left, top, left + width, top + height};
    return rect;
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
        return L"No PNG imported. Import a transparent PNG to use image mode.";
    }

    std::wstring filename = settings.customImagePath;
    const size_t slashIndex = filename.find_last_of(L"\\/");
    if (slashIndex != std::wstring::npos)
    {
        filename = filename.substr(slashIndex + 1);
    }

    return L"Imported PNG: " + filename;
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

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kAppWindowClass,
        kAppName,
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
}

void MainWindow::ShowAndFocus()
{
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
    SendMessageW(generalVisibleCheck_, BM_SETCHECK, settings.visible ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(styleCombo_, CB_SETCURSEL, StyleToInt(settings.style), 0);
    SendMessageW(lengthSlider_, TBM_SETPOS, TRUE, settings.length);
    SendMessageW(gapSlider_, TBM_SETPOS, TRUE, settings.gap);
    SendMessageW(thicknessSlider_, TBM_SETPOS, TRUE, settings.thickness);
    SendMessageW(opacitySlider_, TBM_SETPOS, TRUE, settings.opacity);
    SendMessageW(imageSizeSlider_, TBM_SETPOS, TRUE, settings.imageSize);
    SendMessageW(useCustomColorCheck_, BM_SETCHECK, settings.useCustomColor ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(enableCustomImageCheck_, BM_SETCHECK, settings.renderMode == RenderMode::CustomImage ? BST_CHECKED : BST_UNCHECKED, 0);

    UpdateSliderLabels(settings);
    UpdateImageStatus(settings);
    SetWindowTextW(generalStorageLabel_, (L"Settings location\r\n" + app_->settingsPath()).c_str());

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

    case WM_HSCROLL:
        HandleScroll(wParam, lParam);
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
        return reinterpret_cast<LRESULT>(panelBrush_);
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

    generalVisibleCheck_ = CreateWindowExW(0, L"BUTTON", L"Overlay enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_GENERAL_VISIBLE), instance_, nullptr);
    generalSummaryLabel_ = CreateWindowExW(0, L"STATIC", L"Potato Crosshair launches with the settings window open and a tray icon active.\r\nMinimize and close both hide this window to tray.\r\nLeft-click the tray icon to reopen settings; right-click for quick actions.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    generalStorageLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    previewControl_ = CreateWindowExW(0, kPreviewWindowClass, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_PREVIEW_CONTROL), instance_, this);
    styleCombo_ = CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_STYLE_COMBO), instance_, nullptr);
    SendMessageW(styleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cross"));
    SendMessageW(styleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dot"));
    SendMessageW(styleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Circle"));
    SendMessageW(styleCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"T-Shape"));

    lengthSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_LENGTH_SLIDER), instance_, nullptr);
    gapSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_GAP_SLIDER), instance_, nullptr);
    thicknessSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_THICKNESS_SLIDER), instance_, nullptr);
    opacitySlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_OPACITY_SLIDER), instance_, nullptr);
    SendMessageW(lengthSlider_, TBM_SETRANGE, TRUE, MAKELPARAM(4, 120));
    SendMessageW(gapSlider_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 80));
    SendMessageW(thicknessSlider_, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
    SendMessageW(opacitySlider_, TBM_SETRANGE, TRUE, MAKELPARAM(40, 255));

    lengthValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    gapValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    thicknessValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    opacityValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    for (size_t index = 0; index < swatchButtons_.size(); ++index)
    {
        swatchButtons_[index] = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SWATCH_BASE + static_cast<int>(index))), instance_, nullptr);
    }

    useCustomColorCheck_ = CreateWindowExW(0, L"BUTTON", L"Use custom color", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_USE_CUSTOM_COLOR), instance_, nullptr);
    pickColorButton_ = CreateWindowExW(0, L"BUTTON", L"Choose Color", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_PICK_COLOR_BUTTON), instance_, nullptr);
    customColorSwatch_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_CUSTOM_COLOR_SWATCH), instance_, nullptr);

    enableCustomImageCheck_ = CreateWindowExW(0, L"BUTTON", L"Use imported PNG as the active reticle", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_ENABLE_CUSTOM_IMAGE), instance_, nullptr);
    importImageButton_ = CreateWindowExW(0, L"BUTTON", L"Import PNG", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_IMPORT_IMAGE), instance_, nullptr);
    clearImageButton_ = CreateWindowExW(0, L"BUTTON", L"Clear PNG", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_CLEAR_IMAGE), instance_, nullptr);
    imageStatusLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    imageSizeSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(ID_IMAGE_SIZE_SLIDER), instance_, nullptr);
    SendMessageW(imageSizeSlider_, TBM_SETRANGE, TRUE, MAKELPARAM(16, 512));
    imageSizeValue_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    hotkeysText_ = CreateWindowExW(0, L"STATIC", L"F8  Toggle overlay\r\nF9  Cycle built-in style\r\nCtrl + Alt + C  Cycle preset color\r\nCtrl + Alt + Up / Down  Length\r\nCtrl + Alt + Left / Right  Gap\r\nCtrl + Alt + PageUp / PageDown  Thickness\r\nCtrl + Alt + Home / End  Opacity\r\nCtrl + Alt + R  Reset defaults\r\nCtrl + Alt + Q  Quit", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
    aboutText_ = CreateWindowExW(0, L"STATIC", L"Potato Crosshair\r\nA desktop crosshair overlay with a tray-first workflow, potato-toned shell, built-in reticles, free custom color support, and imported PNG reticles.\r\n\r\nPrimary monitor targeting is preserved from the current MVP.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    for (const HWND control : sectionButtons_)
    {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
    }

    const std::array<HWND, 16> bodyControls = {
        headerMinimizeButton_, headerCloseButton_, generalVisibleCheck_, generalSummaryLabel_, generalStorageLabel_, previewControl_,
        styleCombo_, lengthValue_, gapValue_, thicknessValue_, opacityValue_, useCustomColorCheck_,
        pickColorButton_, customColorSwatch_, imageStatusLabel_, imageSizeValue_};

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
    RegisterSectionControl(styleCombo_, AppSection::Crosshair);
    RegisterSectionControl(lengthSlider_, AppSection::Crosshair);
    RegisterSectionControl(lengthValue_, AppSection::Crosshair);
    RegisterSectionControl(gapSlider_, AppSection::Crosshair);
    RegisterSectionControl(gapValue_, AppSection::Crosshair);
    RegisterSectionControl(thicknessSlider_, AppSection::Crosshair);
    RegisterSectionControl(thicknessValue_, AppSection::Crosshair);
    RegisterSectionControl(opacitySlider_, AppSection::Crosshair);
    RegisterSectionControl(opacityValue_, AppSection::Crosshair);
    RegisterSectionControl(useCustomColorCheck_, AppSection::Crosshair);
    RegisterSectionControl(pickColorButton_, AppSection::Crosshair);
    RegisterSectionControl(customColorSwatch_, AppSection::Crosshair);
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

void MainWindow::LayoutChildren(const int width, const int)
{
    const int sidebarTop = kHeaderHeight + 24;
    const int contentLeft = kSidebarWidth + 30;
    const int contentWidth = width - contentLeft - 24;

    MoveWindow(headerMinimizeButton_, width - 104, 20, 36, 32, TRUE);
    MoveWindow(headerCloseButton_, width - 60, 20, 36, 32, TRUE);

    int sectionTop = sidebarTop;
    for (const HWND button : sectionButtons_)
    {
        MoveWindow(button, 18, sectionTop, kSidebarWidth - 36, kSidebarButtonHeight, TRUE);
        sectionTop += kSidebarButtonHeight + kSidebarGap;
    }

    MoveWindow(generalVisibleCheck_, contentLeft + 24, 160, 220, 28, TRUE);
    MoveWindow(generalSummaryLabel_, contentLeft + 24, 210, contentWidth - 48, 100, TRUE);
    MoveWindow(generalStorageLabel_, contentLeft + 24, 338, contentWidth - 48, 80, TRUE);

    MoveWindow(previewControl_, contentLeft + 24, 156, 280, 280, TRUE);
    MoveWindow(styleCombo_, contentLeft + 356, 176, 220, 300, TRUE);
    MoveWindow(lengthSlider_, contentLeft + 356, 240, 240, 32, TRUE);
    MoveWindow(lengthValue_, contentLeft + 612, 236, 180, 24, TRUE);
    MoveWindow(gapSlider_, contentLeft + 356, 294, 240, 32, TRUE);
    MoveWindow(gapValue_, contentLeft + 612, 290, 180, 24, TRUE);
    MoveWindow(thicknessSlider_, contentLeft + 356, 348, 240, 32, TRUE);
    MoveWindow(thicknessValue_, contentLeft + 612, 344, 180, 24, TRUE);
    MoveWindow(opacitySlider_, contentLeft + 356, 402, 240, 32, TRUE);
    MoveWindow(opacityValue_, contentLeft + 612, 398, 180, 24, TRUE);

    int swatchLeft = contentLeft + 356;
    for (const HWND swatch : swatchButtons_)
    {
        MoveWindow(swatch, swatchLeft, 462, 30, 30, TRUE);
        swatchLeft += 42;
    }

    MoveWindow(useCustomColorCheck_, contentLeft + 356, 512, 180, 24, TRUE);
    MoveWindow(pickColorButton_, contentLeft + 356, 548, 150, 38, TRUE);
    MoveWindow(customColorSwatch_, contentLeft + 520, 548, 38, 38, TRUE);

    MoveWindow(enableCustomImageCheck_, contentLeft + 24, 160, contentWidth - 48, 28, TRUE);
    MoveWindow(importImageButton_, contentLeft + 24, 220, 136, 40, TRUE);
    MoveWindow(clearImageButton_, contentLeft + 174, 220, 136, 40, TRUE);
    MoveWindow(imageStatusLabel_, contentLeft + 24, 290, contentWidth - 48, 40, TRUE);
    MoveWindow(imageSizeSlider_, contentLeft + 24, 370, 260, 32, TRUE);
    MoveWindow(imageSizeValue_, contentLeft + 300, 366, 220, 24, TRUE);

    MoveWindow(hotkeysText_, contentLeft + 28, 160, contentWidth - 56, 280, TRUE);
    MoveWindow(aboutText_, contentLeft + 28, 160, contentWidth - 56, 220, TRUE);
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
    InvalidateRect(pickColorButton_, nullptr, TRUE);
    InvalidateRect(customColorSwatch_, nullptr, TRUE);
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
    FillRect(dc, &clientRect, windowBrush_);

    const RECT headerRect = MakeRect(0, 0, clientRect.right, kHeaderHeight);
    const RECT sidebarRect = MakeRect(0, kHeaderHeight, kSidebarWidth, clientRect.bottom - kHeaderHeight);
    const RECT contentRect = MakeRect(kSidebarWidth + 6, kHeaderHeight + 8, clientRect.right - kSidebarWidth - 18, clientRect.bottom - kHeaderHeight - 16);

    HBRUSH headerBrush = CreateSolidBrush(kThemePanelSecondary);
    FillRect(dc, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    HBRUSH sidebarBrush = CreateSolidBrush(kThemePanelBackground);
    FillRect(dc, &sidebarRect, sidebarBrush);
    DeleteObject(sidebarBrush);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kThemeText);
    SelectObject(dc, titleFont_);
    TextOutW(dc, 28, 22, L"Potato Crosshair", 17);
    SelectObject(dc, smallFont_);
    SetTextColor(dc, kThemeMutedText);
    TextOutW(dc, 30, 50, L"Overlay control center", 22);
    SelectObject(dc, bodyFont_);
    SetTextColor(dc, kThemeText);
    TextOutW(dc, 24, kHeaderHeight + 10, L"Sections", 8);

    SetTextColor(dc, kThemeAccentHover);
    const wchar_t* currentSection = SectionTitle(activeSection_);
    TextOutW(dc, contentRect.left + 18, kHeaderHeight + 18, currentSection, static_cast<int>(wcslen(currentSection)));

    if (activeSection_ == AppSection::General)
    {
        DrawCard(dc, MakeRect(contentRect.left + 12, 130, contentRect.right - 24, 320));
    }
    else if (activeSection_ == AppSection::Crosshair)
    {
        DrawCard(dc, MakeRect(contentRect.left + 12, 130, 310, 320));
        DrawCard(dc, MakeRect(contentRect.left + 344, 130, contentRect.right - 356, 480));
    }
    else
    {
        DrawCard(dc, MakeRect(contentRect.left + 12, 130, contentRect.right - 24, 340));
    }

    EndPaint(hwnd_, &paint);
}

void MainWindow::PaintPreview(const HWND preview) const
{
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(preview, &paint);

    RECT clientRect{};
    GetClientRect(preview, &clientRect);
    const HBRUSH brush = CreateSolidBrush(kThemePanelSecondary);
    FillRect(dc, &clientRect, brush);
    DeleteObject(brush);

    const HPEN borderPen = CreatePen(PS_SOLID, 1, kThemeBorder);
    const HGDIOBJ previousPen = SelectObject(dc, borderPen);
    const HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);
    DeleteObject(borderPen);

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const RECT inner = InflateRectCopy(clientRect, -18);
    DrawReticle(graphics, app_->settings(), Gdiplus::RectF(static_cast<float>(inner.left), static_cast<float>(inner.top), static_cast<float>(inner.right - inner.left), static_cast<float>(inner.bottom - inner.top)));

    EndPaint(preview, &paint);
}

void MainWindow::DrawOwnerButton(const DRAWITEMSTRUCT& drawItem) const
{
    const int controlId = static_cast<int>(drawItem.CtlID);
    const bool selected = (drawItem.itemState & ODS_SELECTED) != 0;
    const bool disabled = (drawItem.itemState & ODS_DISABLED) != 0;

    COLORREF fillColor = kThemePanelSecondary;
    COLORREF borderColor = kThemeBorder;
    COLORREF textColor = disabled ? kThemeMutedText : kThemeText;

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
        fillColor = app_->settings().customColor;
        borderColor = app_->settings().useCustomColor ? kThemeAccentHover : kThemeBorder;
    }
    else if (controlId >= ID_SWATCH_BASE && controlId < ID_SWATCH_BASE + static_cast<int>(kColorPresets.size()))
    {
        fillColor = GetPresetColor(controlId - ID_SWATCH_BASE);
        borderColor = (!app_->settings().useCustomColor && app_->settings().colorIndex == controlId - ID_SWATCH_BASE) ? kThemeAccentHover : kThemeBorder;
    }

    const HBRUSH brush = CreateSolidBrush(fillColor);
    const HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    const HGDIOBJ previousBrush = SelectObject(drawItem.hDC, brush);
    const HGDIOBJ previousPen = SelectObject(drawItem.hDC, pen);
    RoundRect(drawItem.hDC, drawItem.rcItem.left, drawItem.rcItem.top, drawItem.rcItem.right, drawItem.rcItem.bottom, 10, 10);
    SelectObject(drawItem.hDC, previousBrush);
    SelectObject(drawItem.hDC, previousPen);
    DeleteObject(brush);
    DeleteObject(pen);

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
        updated.visible = SendMessageW(generalVisibleCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_STYLE_COMBO && notification == CBN_SELCHANGE)
    {
        updated.style = IntToStyle(static_cast<int>(SendMessageW(styleCombo_, CB_GETCURSEL, 0, 0)));
        app_->ApplySettings(updated);
        return;
    }
    if (controlId == ID_USE_CUSTOM_COLOR && notification == BN_CLICKED)
    {
        updated.useCustomColor = SendMessageW(useCustomColorCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
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
        updated.renderMode = SendMessageW(enableCustomImageCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED && !updated.customImagePath.empty() ? RenderMode::CustomImage : RenderMode::BuiltIn;
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

void MainWindow::HandleScroll(const WPARAM, const LPARAM lParam)
{
    if (updatingControls_)
    {
        return;
    }

    const HWND slider = reinterpret_cast<HWND>(lParam);
    Settings updated = CopySettings();

    if (slider == lengthSlider_)
    {
        updated.length = static_cast<int>(SendMessageW(lengthSlider_, TBM_GETPOS, 0, 0));
    }
    else if (slider == gapSlider_)
    {
        updated.gap = static_cast<int>(SendMessageW(gapSlider_, TBM_GETPOS, 0, 0));
    }
    else if (slider == thicknessSlider_)
    {
        updated.thickness = static_cast<int>(SendMessageW(thicknessSlider_, TBM_GETPOS, 0, 0));
    }
    else if (slider == opacitySlider_)
    {
        updated.opacity = static_cast<int>(SendMessageW(opacitySlider_, TBM_GETPOS, 0, 0));
    }
    else if (slider == imageSizeSlider_)
    {
        updated.imageSize = static_cast<int>(SendMessageW(imageSizeSlider_, TBM_GETPOS, 0, 0));
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
