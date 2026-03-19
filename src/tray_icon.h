#pragma once

#include <windows.h>
#include <shellapi.h>

namespace potato
{
inline constexpr UINT kTrayCallbackMessage = WM_APP + 41;

class TrayIcon
{
public:
    bool Create(HWND owner, HINSTANCE instance);
    void Destroy();
    void Recreate();
    void UpdateTooltip(bool overlayVisible);
    void ShowContextMenu(bool overlayVisible) const;

private:
    NOTIFYICONDATAW data_{};
    bool created_ = false;
};
} // namespace potato
