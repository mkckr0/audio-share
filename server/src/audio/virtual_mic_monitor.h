#ifndef AUDIOSTREAM_VIRTUAL_MIC_MONITOR_H
#define AUDIOSTREAM_VIRTUAL_MIC_MONITOR_H

#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>

namespace audiostream::audio {

enum class MicMonitorState {
    IDLE,
    ACTIVE,
    DEBOUNCE_WAIT
};

using MicStateChangeCallback = std::function<void(bool active)>;

class VirtualMicMonitor {
public:
    VirtualMicMonitor(double debounce_seconds = 3.0);
    ~VirtualMicMonitor();

    bool start(MicStateChangeCallback callback, const std::string& virtual_device_name = "CABLE Output");
    void stop();

    bool is_monitoring() const { return is_monitoring_; }
    bool is_mic_active() const { return current_state_ == MicMonitorState::ACTIVE; }
    int get_active_sessions_count() const { return active_sessions_count_; }

    // Manually simulate or inject active session count for testing/control
    void set_session_count(int count);

private:
    void monitor_thread_func();
    int query_windows_session_count();

    std::atomic<bool> is_monitoring_{false};
    std::thread monitor_thread_;
    MicStateChangeCallback state_callback_;
    std::string device_name_;
    double debounce_seconds_ = 3.0;

    std::atomic<MicMonitorState> current_state_{MicMonitorState::IDLE};
    std::atomic<int> active_sessions_count_{0};
    std::chrono::steady_clock::time_point debounce_start_time_;
};

} // namespace audiostream::audio

#endif // AUDIOSTREAM_VIRTUAL_MIC_MONITOR_H
