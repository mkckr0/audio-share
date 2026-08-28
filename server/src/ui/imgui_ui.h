#ifndef AUDIOSTREAM_IMGUI_UI_H
#define AUDIOSTREAM_IMGUI_UI_H

#include <vector>
#include <string>
#include <deque>
#include "../net/network_manager.h"
#include "../audio/virtual_mic_monitor.h"
#include "../audio/wasapi_capture.h"

namespace audiostream::ui {

struct UiState {
    bool is_running = true;
    bool is_tethered = false;
    bool is_mic_active = false;
    int active_mic_sessions = 0;
    float volume_master = 1.0f;
    float vu_left_db = -60.0f;
    float vu_right_db = -60.0f;
    int selected_codec_idx = 1; // 0=PCM, 1=Opus
    int selected_bitrate_idx = 2; // 128 kbps
    int selected_latency_preset = 1; // 0=Low, 1=Balanced, 2=High
    int selected_device_idx = 0;
    std::vector<std::string> audio_devices;
    std::deque<float> latency_history;
    std::deque<float> jitter_history;
};

class AudioStreamUi {
public:
    AudioStreamUi();
    ~AudioStreamUi() = default;

    void apply_dark_theme();
    void render(UiState& state, net::NetworkManager& net_mgr, audio::VirtualMicMonitor& mic_mon);

private:
    void render_header(UiState& state);
    void render_dashboard_tab(UiState& state, net::NetworkManager& net_mgr, audio::VirtualMicMonitor& mic_mon);
    void render_codec_tab(UiState& state, net::NetworkManager& net_mgr);
    void render_network_tab(UiState& state, net::NetworkManager& net_mgr);
    void render_about_tab();
};

} // namespace audiostream::ui

#endif // AUDIOSTREAM_IMGUI_UI_H
