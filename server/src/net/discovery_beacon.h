#ifndef AUDIOSTREAM_DISCOVERY_BEACON_H
#define AUDIOSTREAM_DISCOVERY_BEACON_H

#include <string>
#include <atomic>
#include <thread>
#include "audiostream.pb.h"

namespace audiostream::net {

class DiscoveryBeaconSender {
public:
    DiscoveryBeaconSender(uint16_t discovery_port = 59200, uint16_t server_tcp_port = 65530);
    ~DiscoveryBeaconSender();

    bool start(const std::string& server_name = "AudioStream Desktop Server");
    void stop();

    void set_server_ip(const std::string& ip) { server_ip_ = ip; }
    bool is_running() const { return is_running_; }

private:
    void beacon_thread_func();

    uint16_t discovery_port_ = 59200;
    uint16_t server_tcp_port_ = 65530;
    std::string server_name_;
    std::string server_ip_ = "0.0.0.0";
    std::atomic<bool> is_running_{false};
    std::thread beacon_thread_;
};

} // namespace audiostream::net

#endif // AUDIOSTREAM_DISCOVERY_BEACON_H
