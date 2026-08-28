#include "tray_icon.h"
#include <cstring>

namespace audiostream::ui {

SystemTrayIcon::SystemTrayIcon() = default;

SystemTrayIcon::~SystemTrayIcon() {
#if defined(_WIN32)
    destroy();
#endif
}

#if defined(_WIN32)
bool SystemTrayIcon::create(HWND hwnd, UINT uID, HICON hIcon, const std::wstring& tooltip) {
    if (created_) destroy();
    hwnd_ = hwnd;
    uid_ = uID;

    memset(&nid_, 0, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = uid_;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    wcsncpy(nid_.szTip, tooltip.c_str(), 127);

    created_ = Shell_NotifyIconW(NIM_ADD, &nid_);
    return created_;
}

void SystemTrayIcon::destroy() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void SystemTrayIcon::show_balloon(const std::wstring& title, const std::wstring& text, DWORD flags) {
    if (!created_) return;
    nid_.uFlags |= NIF_INFO;
    nid_.dwInfoFlags = flags;
    wcsncpy(nid_.szInfoTitle, title.c_str(), 63);
    wcsncpy(nid_.szInfo, text.c_str(), 255);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void SystemTrayIcon::set_tooltip(const std::wstring& tooltip) {
    if (!created_) return;
    wcsncpy(nid_.szTip, tooltip.c_str(), 127);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}
#endif

} // namespace audiostream::ui
