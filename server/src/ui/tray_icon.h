#ifndef AUDIOSTREAM_TRAY_ICON_H
#define AUDIOSTREAM_TRAY_ICON_H

#include <string>
#include <functional>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#define WM_TRAYICON (WM_USER + 1)
#endif

namespace audiostream::ui {

using TrayCommandCallback = std::function<void(int cmd_id)>;

class SystemTrayIcon {
public:
    SystemTrayIcon();
    ~SystemTrayIcon();

#if defined(_WIN32)
    bool create(HWND hwnd, UINT uID, HICON hIcon, const std::wstring& tooltip);
    void destroy();
    void show_balloon(const std::wstring& title, const std::wstring& text, DWORD flags = NIIF_INFO);
    void set_tooltip(const std::wstring& tooltip);
    bool is_created() const { return created_; }
#else
    bool create(void* hwnd, unsigned int uID, void* hIcon, const std::wstring& tooltip) { return true; }
    void destroy() {}
    void show_balloon(const std::wstring& title, const std::wstring& text, unsigned int flags = 0) {}
    void set_tooltip(const std::wstring& tooltip) {}
    bool is_created() const { return true; }
#endif

private:
#if defined(_WIN32)
    HWND hwnd_ = nullptr;
    UINT uid_ = 0;
    NOTIFYICONDATAW nid_{};
    bool created_ = false;
#endif
};

} // namespace audiostream::ui

#endif // AUDIOSTREAM_TRAY_ICON_H
