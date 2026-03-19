#include "app_controller.h"

#include "main_window.h"

#include <commctrl.h>

namespace potato
{
AppController::AppController() = default;

AppController::~AppController()
{
    delete mainWindow_;
    mainWindow_ = nullptr;
}

int AppController::Run(const HINSTANCE instance, const int showCommand)
{
    instance_ = instance;
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    if (!settingsStore_.Initialize(instance_))
    {
        MessageBoxW(nullptr, L"Could not initialize the app data directory.", kAppName, MB_ICONERROR | MB_OK);
        return -1;
    }

    settings_ = settingsStore_.Load();

    overlayWindow_.SetSettingsChangedCallback([this](const Settings& settings)
    {
        ApplySettings(settings, true);
    });
    overlayWindow_.SetQuitRequestedCallback([this]()
    {
        RequestQuit();
    });

    if (!overlayWindow_.Create(instance_))
    {
        MessageBoxW(nullptr, L"Could not create the overlay window.", kAppName, MB_ICONERROR | MB_OK);
        return -1;
    }

    mainWindow_ = new MainWindow(this);
    if (!mainWindow_->Create(instance_))
    {
        MessageBoxW(nullptr, L"Could not create the main window.", kAppName, MB_ICONERROR | MB_OK);
        return -1;
    }

    if (!trayIcon_.Create(mainWindow_->hwnd(), instance_))
    {
        MessageBoxW(nullptr, L"Could not create the system tray icon.", kAppName, MB_ICONERROR | MB_OK);
        return -1;
    }

    ApplySettings(settings_, false);
    mainWindow_->Show(showCommand);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    trayIcon_.Destroy();
    overlayWindow_.Destroy();
    return static_cast<int>(message.wParam);
}

const Settings& AppController::settings() const
{
    return settings_;
}

std::wstring AppController::settingsPath() const
{
    return settingsStore_.GetSettingsPath();
}

std::wstring AppController::assetsDirectory() const
{
    return settingsStore_.GetAssetsDirectory();
}

void AppController::ApplySettings(const Settings& settings, const bool saveChanges)
{
    Settings sanitized = settings;
    sanitized.length = ClampInt(sanitized.length, 4, 120);
    sanitized.gap = ClampInt(sanitized.gap, 0, 80);
    sanitized.thickness = ClampInt(sanitized.thickness, 1, 20);
    sanitized.opacity = ClampInt(sanitized.opacity, 40, 255);
    sanitized.colorIndex = ClampInt(sanitized.colorIndex, 0, static_cast<int>(kColorPresets.size()) - 1);
    sanitized.imageSize = ClampInt(sanitized.imageSize, 16, 512);

    if (sanitized.renderMode == RenderMode::CustomImage && sanitized.customImagePath.empty())
    {
        sanitized.renderMode = RenderMode::BuiltIn;
    }

    settings_ = sanitized;

    if (saveChanges)
    {
        settingsStore_.Save(settings_);
    }

    overlayWindow_.ApplySettings(settings_);
    trayIcon_.UpdateTooltip(settings_.visible);

    if (mainWindow_ != nullptr)
    {
        mainWindow_->ApplySettings(settings_);
    }
}

void AppController::ToggleOverlayVisibility()
{
    Settings updated = settings_;
    updated.visible = !updated.visible;
    ApplySettings(updated);
}

void AppController::ShowSettingsWindow()
{
    if (mainWindow_ != nullptr)
    {
        mainWindow_->ShowAndFocus();
    }
}

void AppController::HideSettingsWindow()
{
    if (mainWindow_ != nullptr)
    {
        mainWindow_->HideToTray();
    }
}

void AppController::RequestQuit()
{
    if (quitting_)
    {
        return;
    }

    quitting_ = true;
    trayIcon_.Destroy();

    if (mainWindow_ != nullptr)
    {
        mainWindow_->Destroy();
    }

    overlayWindow_.Destroy();
    PostQuitMessage(0);
}

void AppController::RecreateTrayIcon()
{
    trayIcon_.Recreate();
    trayIcon_.UpdateTooltip(settings_.visible);
}

bool AppController::ImportCustomImage(const std::wstring& sourcePath)
{
    try
    {
        const auto importedPath = settingsStore_.ImportCustomImage(sourcePath);
        Settings updated = settings_;
        updated.customImagePath = importedPath.wstring();
        updated.renderMode = RenderMode::CustomImage;
        ApplySettings(updated);
        return true;
    }
    catch (...)
    {
        MessageBoxW(mainWindow_ != nullptr ? mainWindow_->hwnd() : nullptr, L"Could not import the selected PNG.", kAppName, MB_ICONERROR | MB_OK);
        return false;
    }
}

void AppController::ClearCustomImage()
{
    settingsStore_.ClearCustomImage();

    Settings updated = settings_;
    updated.customImagePath.clear();
    updated.renderMode = RenderMode::BuiltIn;
    ApplySettings(updated);
}

HINSTANCE AppController::instance() const
{
    return instance_;
}

TrayIcon& AppController::tray()
{
    return trayIcon_;
}

bool AppController::isQuitting() const
{
    return quitting_;
}
} // namespace potato
