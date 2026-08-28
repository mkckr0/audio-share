#ifndef AUDIOSTREAM_D3D11_BACKEND_H
#define AUDIOSTREAM_D3D11_BACKEND_H

#include <string>
#include <functional>

#if defined(_WIN32)
#include <windows.h>
#include <d3d11.h>
#endif

namespace audiostream::ui {

using RenderCallback = std::function<void()>;

class D3d11AppWindow {
public:
    D3d11AppWindow();
    ~D3d11AppWindow();

    bool initialize(const std::wstring& title, int width = 720, int height = 480);
    void run_loop(RenderCallback render_cb);
    void cleanup();

#if defined(_WIN32)
    HWND get_hwnd() const { return hwnd_; }
#endif

private:
#if defined(_WIN32)
    HWND hwnd_ = nullptr;
    ID3D11Device* pd3dDevice_ = nullptr;
    ID3D11DeviceContext* pd3dDeviceContext_ = nullptr;
    IDXGISwapChain* pSwapChain_ = nullptr;
    ID3D11RenderTargetView* mainRenderTargetView_ = nullptr;

    bool create_device_d3d(HWND hWnd);
    void cleanup_device_d3d();
    void create_render_target();
    void cleanup_render_target();
#endif
    bool is_initialized_ = false;
};

} // namespace audiostream::ui

#endif // AUDIOSTREAM_D3D11_BACKEND_H
