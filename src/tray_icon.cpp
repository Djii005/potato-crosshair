#include "tray_icon.h"

#include "app_types.h"
#include "resource.h"

namespace potato
{
bool TrayIcon::Create(const HWND owner, const HINSTANCE instance)
{
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    data_.uCallbackMessage = kTrayCallbackMessage;
    data_.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    wcscpy_s(data_.szTip, L"Potato Crosshair");

    created_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (created_)
    {
        data_.uVersion = NOTIFYICON_VERSION;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }

    return created_;
}

void TrayIcon::Destroy()
{
    if (created_)
    {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        created_ = false;
    }

    if (data_.hIcon != nullptr)
    {
        DestroyIcon(data_.hIcon);
        data_.hIcon = nullptr;
    }
}

void TrayIcon::Recreate()
{
    if (!created_)
    {
        return;
    }

    Shell_NotifyIconW(NIM_ADD, &data_);
    data_.uVersion = NOTIFYICON_VERSION;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
}

void TrayIcon::UpdateTooltip(const bool overlayVisible)
{
    if (!created_)
    {
        return;
    }

    wcscpy_s(data_.szTip, overlayVisible ? L"Potato Crosshair - Overlay On" : L"Potato Crosshair - Overlay Off");
    data_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &data_);
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void TrayIcon::ShowContextMenu(const bool overlayVisible) const
{
    if (!created_)
    {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open Settings");
    AppendMenuW(menu, MF_STRING, ID_TRAY_TOGGLE, overlayVisible ? L"Hide Overlay" : L"Show Overlay");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"Quit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(data_.hWnd);
    const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, data_.hWnd, nullptr);
    if (selected != 0)
    {
        PostMessageW(data_.hWnd, WM_COMMAND, MAKEWPARAM(selected, 0), 0);
    }

    DestroyMenu(menu);
}
} // namespace potato
