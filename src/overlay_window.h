#pragma once

#include "app_types.h"

#include <functional>

namespace potato
{
class OverlayWindow
{
public:
    using SettingsChangedCallback = std::function<void(const Settings&)>;
    using QuitRequestedCallback = std::function<void()>;

    bool Create(HINSTANCE instance);
    void Destroy();

    void ApplySettings(const Settings& settings);
    void SetSettingsChangedCallback(SettingsChangedCallback callback);
    void SetQuitRequestedCallback(QuitRequestedCallback callback);

    HWND hwnd() const;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void RegisterHotkeys();
    void UnregisterHotkeys() const;
    void HandleHotkey(UINT hotkey);
    void RefreshOverlayPlacement();
    void Render() const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0;
    Settings settings_{};
    SettingsChangedCallback settingsChangedCallback_;
    QuitRequestedCallback quitRequestedCallback_;
};
} // namespace potato
