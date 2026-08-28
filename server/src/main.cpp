#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>

#include "config.h"
#include "audio/wasapi_capture.h"
#include "audio/wasapi_render.h"
#include "audio/virtual_mic_monitor.h"
#include "codec/opus_encoder.h"
#include "codec/opus_decoder.h"
#include "net/network_manager.h"
#include "net/usb_tether_monitor.h"
#include "net/discovery_beacon.h"
#include "ui/imgui_ui.h"
#include "ui/d3d11_backend.h"
#include "ui/tray_icon.h"

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace audiostream;

int main(int argc, char* argv[]) {
    bool headless = false;
    uint16_t port = 65530;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "-h") {
            headless = true;
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "AudioStream v" << AUDIOSTREAM_VERSION << std::endl;
            return 0;
        }
    }

    std::cout << "Starting AudioStream Desktop Server v" << AUDIOSTREAM_VERSION << " on port " << port << "..." << std::endl;

    // 1. Audio Engine & Codecs
    codec::AudioOpusEncoder pcm_opus_encoder(48000, 2, 128000);
    audio::WasapiAudioCapture loopback_capture;
    audio::WasapiAudioRender virtual_mic_feeder;
    audio::VirtualMicMonitor mic_monitor(3.0);

    // 2. Networking & Discovery
    net::NetworkManager network_mgr(port);
    net::DiscoveryBeaconSender beacon_sender(59200, port);
    net::UsbTetherMonitor tether_mon;

    // 3. UI State
    ui::UiState ui_state;
    ui_state.audio_devices.push_back("Default Loopback Device");
    for (int i = 0; i < 60; ++i) {
        ui_state.latency_history.push_back(15.0f + (i % 5) * 1.2f);
        ui_state.jitter_history.push_back(1.5f + (i % 3) * 0.4f);
    }

    // Connect Phone Mic Receiver -> VB-CABLE Input
    virtual_mic_feeder.initialize("CABLE Input", 48000, 1);
    auto phone_mic_cb = [&virtual_mic_feeder](const uint8_t* pcm_data, size_t bytes_len) {
        virtual_mic_feeder.render_samples(reinterpret_cast<const int16_t*>(pcm_data), bytes_len / sizeof(int16_t));
    };

    if (!network_mgr.start(phone_mic_cb)) {
        std::cerr << "Failed to start network manager on port " << port << std::endl;
        return 1;
    }

    // Connect Loopback Audio Capture -> Opus Encoder -> UDP Broadcast
    uint32_t sample_timestamp = 0;
    auto audio_data_cb = [&network_mgr, &pcm_opus_encoder, &sample_timestamp, &ui_state](const uint8_t* pcm, size_t bytes, const audio::AudioFormatDesc& fmt) {
        const int frame_samples_per_channel = 960; // 20ms
        uint8_t opus_buf[1500];
        
        // Calculate peak volume for UI meters
        const int16_t* s16 = reinterpret_cast<const int16_t*>(pcm);
        size_t total_samples = bytes / sizeof(int16_t);
        int max_l = 0, max_r = 0;
        for (size_t i = 0; i + 1 < total_samples; i += 2) {
            if (std::abs(s16[i]) > max_l) max_l = std::abs(s16[i]);
            if (std::abs(s16[i+1]) > max_r) max_r = std::abs(s16[i+1]);
        }
        ui_state.vu_left_db = max_l > 0 ? 20.0f * std::log10(max_l / 32767.0f) : -60.0f;
        ui_state.vu_right_db = max_r > 0 ? 20.0f * std::log10(max_r / 32767.0f) : -60.0f;

        auto active_cfg = network_mgr.get_active_config();
        if (active_cfg.codec() == com::audiostream::pb::CODEC_OPUS) {
            int enc_bytes = pcm_opus_encoder.encode_int16(s16, frame_samples_per_channel, opus_buf, sizeof(opus_buf));
            if (enc_bytes > 0) {
                network_mgr.broadcast_audio_frame(opus_buf, enc_bytes, sample_timestamp);
            }
        } else {
            // Lossless raw PCM transmission
            network_mgr.broadcast_audio_frame(pcm, bytes, sample_timestamp);
        }
        sample_timestamp += frame_samples_per_channel;
    };

    loopback_capture.start(audio_data_cb);

    // Connect Virtual Mic Monitor -> Notify Android Phone Mic Activation
    mic_monitor.start([&network_mgr, &ui_state](bool active) {
        ui_state.is_mic_active = active;
        if (active) {
            std::cout << "[VirtualMicMonitor] Active capture session detected -> Sending CMD_START_MIC" << std::endl;
            network_mgr.notify_start_mic(7); // VOICE_COMMUNICATION
        } else {
            std::cout << "[VirtualMicMonitor] Session closed & debounce expired -> Sending CMD_STOP_MIC" << std::endl;
            network_mgr.notify_stop_mic();
        }
    });

    // Connect USB Tether Monitor -> Beacon & Telemetry
    tether_mon.start([&beacon_sender, &ui_state](bool tethered, const std::string& ip) {
        ui_state.is_tethered = tethered;
        beacon_sender.set_server_ip(ip);
        std::cout << "[UsbTetherMonitor] Network state changed: " << (tethered ? "USB Tethered (" + ip + ")" : "Wi-Fi / LAN (" + ip + ")") << std::endl;
    });

    beacon_sender.set_server_ip(tether_mon.get_preferred_ip());
    beacon_sender.start("AudioStream Server");

    std::cout << "AudioStream Server is running. Press Ctrl+C or close window to exit." << std::endl;

#if defined(_WIN32)
    if (!headless) {
        ui::D3d11AppWindow app_window;
        ui::AudioStreamUi app_ui;
        ui::SystemTrayIcon tray_icon;

        if (app_window.initialize(L"AudioStream Desktop Server", 800, 560)) {
            app_ui.apply_dark_theme();
            tray_icon.create(app_window.get_hwnd(), 1001, NULL, L"AudioStream Desktop Server");

            app_window.run_loop([&]() {
                ui_state.active_mic_sessions = mic_monitor.get_active_sessions_count();
                app_ui.render(ui_state, network_mgr, mic_monitor);
            });

            tray_icon.destroy();
            app_window.cleanup();
        }
    } else {
        while (ui_state.is_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
#else
    // Headless / Linux test mode
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif

    // Clean shutdown
    beacon_sender.stop();
    tether_mon.stop();
    mic_monitor.stop();
    loopback_capture.stop();
    virtual_mic_feeder.destroy();
    network_mgr.stop();

    std::cout << "AudioStream Server shut down gracefully." << std::endl;
    return 0;
}
