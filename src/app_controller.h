#pragma once

#include "app_types.h"
#include "overlay_window.h"
#include "settings_store.h"
#include "tray_icon.h"

namespace potato
{
class MainWindow;

class AppController
{
public:
    AppController();
    ~AppController();

    int Run(HINSTANCE instance, int showCommand);

    const Settings& settings() const;
    std::wstring settingsPath() const;
    std::wstring assetsDirectory() const;

    void ApplySettings(const Settings& settings, bool saveChanges = true);
    void ToggleOverlayVisibility();
    void ShowSettingsWindow();
    void HideSettingsWindow();
    void RequestQuit();
    void RecreateTrayIcon();

    bool ImportCustomImage(const std::wstring& sourcePath);
    void ClearCustomImage();

    HINSTANCE instance() const;
    TrayIcon& tray();
    bool isQuitting() const;

private:
    HINSTANCE instance_ = nullptr;
    bool quitting_ = false;
    Settings settings_{};
    SettingsStore settingsStore_;
    OverlayWindow overlayWindow_;
    TrayIcon trayIcon_;
    MainWindow* mainWindow_ = nullptr;
};
} // namespace potato
