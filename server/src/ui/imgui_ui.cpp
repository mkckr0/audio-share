#include "imgui_ui.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace audiostream::ui {

AudioStreamUi::AudioStreamUi() = default;

void AudioStreamUi::apply_dark_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.52f, 0.56f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_Border]                = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.45f, 0.75f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.55f, 0.88f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.16f, 0.38f, 0.65f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.32f, 0.38f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.34f, 0.39f, 0.46f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.20f, 0.75f, 0.45f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.30f, 0.90f, 0.55f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.20f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.22f, 0.48f, 0.80f, 1.00f);
}

void AudioStreamUi::render(UiState& state, net::NetworkManager& net_mgr, audio::VirtualMicMonitor& mic_mon) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("AudioStreamMainWindow", nullptr, flags)) {
        render_header(state);

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Dashboard")) {
                render_dashboard_tab(state, net_mgr, mic_mon);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio & Codecs")) {
                render_codec_tab(state, net_mgr);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Network & Tethering")) {
                render_network_tab(state, net_mgr);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About")) {
                render_about_tab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void AudioStreamUi::render_header(UiState& state) {
    ImGui::TextColored(ImVec4(0.20f, 0.65f, 1.00f, 1.00f), "AudioStream Desktop Server v1.0.0");
    ImGui::SameLine();
    if (state.is_tethered) {
        ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.40f, 1.00f), "[USB Tethering Active]");
    } else {
        ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.00f), "[Wi-Fi / LAN]");
    }
    ImGui::Separator();
}

void AudioStreamUi::render_dashboard_tab(UiState& state, net::NetworkManager& net_mgr, audio::VirtualMicMonitor& mic_mon) {
    ImGui::Text("Audio Output Meters (WASAPI Loopback)");
    
    // Left Channel Meter
    float vu_left_norm = std::clamp((state.vu_left_db + 60.0f) / 60.0f, 0.0f, 1.0f);
    ImGui::ProgressBar(vu_left_norm, ImVec2(-1, 14), "L");

    // Right Channel Meter
    float vu_right_norm = std::clamp((state.vu_right_db + 60.0f) / 60.0f, 0.0f, 1.0f);
    ImGui::ProgressBar(vu_right_norm, ImVec2(-1, 14), "R");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Virtual Microphone Status
    ImGui::Text("Virtual Microphone (Phone Mic -> VB-CABLE Input):");
    ImGui::SameLine();
    if (state.is_mic_active) {
        ImGui::TextColored(ImVec4(0.20f, 0.90f, 0.30f, 1.00f), "● ACTIVE (%d apps capturing)", state.active_mic_sessions);
    } else {
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.00f), "○ IDLE (Auto-activates on Discord/Zoom)");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Latency & Jitter Graph
    ImGui::Text("Network Playout Latency & Jitter Telemetry (ms):");
    if (!state.latency_history.empty()) {
        std::vector<float> lat_vec(state.latency_history.begin(), state.latency_history.end());
        ImGui::PlotLines("Latency", lat_vec.data(), static_cast<int>(lat_vec.size()), 0, "Latency (ms)", 0.0f, 100.0f, ImVec2(-1, 65));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Connected Clients Table
    ImGui::Text("Connected Clients (%zu active):", net_mgr.get_peer_count());
    if (ImGui::BeginTable("PeersTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Session ID");
        ImGui::TableSetupColumn("Client IP");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Codec");
        ImGui::TableSetupColumn("Packets Sent");
        ImGui::TableSetupColumn("Bytes Sent");
        ImGui::TableHeadersRow();

        auto peers = net_mgr.get_connected_peers();
        for (const auto& p : peers) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", p.session_id);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s:%u", p.ip_address.c_str(), p.tcp_port);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(p.is_streaming ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f) : ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
                               p.is_streaming ? "Streaming" : "Connected");
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", p.config.codec() == com::audiostream::pb::CODEC_OPUS ? "Opus VBR" : "Lossless PCM");
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%lu", p.packets_sent);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.2f MB", static_cast<double>(p.bytes_sent) / (1024.0 * 1024.0));
        }
        ImGui::EndTable();
    }
}

void AudioStreamUi::render_codec_tab(UiState& state, net::NetworkManager& net_mgr) {
    ImGui::Text("Codec & Stream Configuration");
    ImGui::Spacing();

    const char* codecs[] = { "Lossless Uncompressed PCM (48kHz 16-bit)", "Opus Compressed Audio (Low Latency VBR)" };
    if (ImGui::Combo("Audio Codec", &state.selected_codec_idx, codecs, IM_ARRAYSIZE(codecs))) {
        auto cfg = net_mgr.get_active_config();
        cfg.set_codec(state.selected_codec_idx == 1 ? com::audiostream::pb::CODEC_OPUS : com::audiostream::pb::CODEC_PCM);
        net_mgr.set_active_config(cfg);
    }

    if (state.selected_codec_idx == 1) {
        const char* bitrates[] = { "64 kbps (Voice / Low Bandwidth)", "96 kbps (Balanced)", "128 kbps (High Quality)", "192 kbps (Ultra HQ)" };
        const int bitrate_values[] = { 64000, 96000, 128000, 192000 };
        if (ImGui::Combo("Opus Bitrate", &state.selected_bitrate_idx, bitrates, IM_ARRAYSIZE(bitrates))) {
            auto cfg = net_mgr.get_active_config();
            cfg.set_opus_bitrate(bitrate_values[state.selected_bitrate_idx]);
            net_mgr.set_active_config(cfg);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* latencies[] = { "Low Latency (20ms buffer - Gaming/Calls)", "Balanced (50ms buffer - Default)", "High Reliability (100ms buffer - Weak Wi-Fi)" };
    const int latency_values[] = { 20, 50, 100 };
    if (ImGui::Combo("Jitter Buffer Latency", &state.selected_latency_preset, latencies, IM_ARRAYSIZE(latencies))) {
        auto cfg = net_mgr.get_active_config();
        cfg.set_target_latency_ms(latency_values[state.selected_latency_preset]);
        net_mgr.set_active_config(cfg);
    }

    ImGui::SliderFloat("Master Stream Volume", &state.volume_master, 0.0f, 2.0f, "%.2fx");
}

void AudioStreamUi::render_network_tab(UiState& state, net::NetworkManager& net_mgr) {
    ImGui::Text("Network & Discovery Settings");
    ImGui::Spacing();

    ImGui::Text("Control Port (TCP): 65530");
    ImGui::Text("Audio Streaming Port (UDP): 65530");
    ImGui::Text("Discovery Beacon Broadcast (UDP): 59200 (every 2.0s)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("USB Tethering Auto-Detection:");
    ImGui::BulletText("Subnets scanned: 192.168.42.0/24 (Android USB RNDIS) & 192.168.137.0/24");
    ImGui::BulletText("Status: %s", state.is_tethered ? "USB Tether Connected" : "Searching / Wi-Fi fallback");
}

void AudioStreamUi::render_about_tab() {
    ImGui::TextColored(ImVec4(0.20f, 0.65f, 1.00f, 1.00f), "AudioStream");
    ImGui::Text("High-performance bidirectional PC/Android audio streaming system.");
    ImGui::Spacing();
    ImGui::Text("Version: 1.0.0");
    ImGui::Text("License: Apache License 2.0");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("AudioStream enables ultra-low latency desktop audio streaming to Android devices with dynamic Opus compression, adaptive jitter buffering, and on-demand phone microphone routing to Windows virtual inputs.");
}

} // namespace audiostream::ui
