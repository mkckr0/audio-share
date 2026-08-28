#ifndef AUDIOSTREAM_USB_TETHER_MONITOR_H
#define AUDIOSTREAM_USB_TETHER_MONITOR_H

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

namespace audiostream::net {

enum class InterfaceType {
    UNKNOWN,
    USB_TETHERING,
    WIFI_LAN,
    LOOPBACK
};

struct NetworkInterfaceInfo {
    std::string name;
    std::string ip_address;
    InterfaceType type = InterfaceType::UNKNOWN;
    bool is_up = true;
};

using TetherStateChangeCallback = std::function<void(bool tethered, const std::string& ip)>;

class UsbTetherMonitor {
public:
    UsbTetherMonitor();
    ~UsbTetherMonitor();

    bool start(TetherStateChangeCallback callback = nullptr);
    void stop();

    std::vector<NetworkInterfaceInfo> scan_interfaces();
    bool is_usb_tethered() const { return is_tethered_; }
    std::string get_preferred_ip() const { return preferred_ip_; }

    static InterfaceType classify_ip(const std::string& ip);

private:
    void monitor_thread_func();

    std::atomic<bool> is_monitoring_{false};
    std::thread monitor_thread_;
    TetherStateChangeCallback callback_;
    std::atomic<bool> is_tethered_{false};
    std::string preferred_ip_ = "127.0.0.1";
};

} // namespace audiostream::net

#endif // AUDIOSTREAM_USB_TETHER_MONITOR_H
