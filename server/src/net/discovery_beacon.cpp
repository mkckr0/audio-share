#include "discovery_beacon.h"
#include <iostream>
#include <chrono>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace audiostream::net {

DiscoveryBeaconSender::DiscoveryBeaconSender(uint16_t discovery_port, uint16_t server_tcp_port)
    : discovery_port_(discovery_port), server_tcp_port_(server_tcp_port) {}

DiscoveryBeaconSender::~DiscoveryBeaconSender() {
    stop();
}

bool DiscoveryBeaconSender::start(const std::string& server_name) {
    if (is_running_) stop();
    server_name_ = server_name;
    is_running_ = true;
    beacon_thread_ = std::thread(&DiscoveryBeaconSender::beacon_thread_func, this);
    return true;
}

void DiscoveryBeaconSender::stop() {
    is_running_ = false;
    if (beacon_thread_.joinable()) {
        beacon_thread_.join();
    }
}

void DiscoveryBeaconSender::beacon_thread_func() {
#if defined(_WIN32)
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return;

    BOOL broadcast = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

    sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(discovery_port_);
    bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    while (is_running_) {
        com::audiostream::pb::DiscoveryBeacon beacon;
        beacon.set_app_name("AudioStream");
        beacon.set_server_name(server_name_);
        beacon.set_port(server_tcp_port_);
        beacon.set_version("1.0.0");
        beacon.set_supports_opus(true);
        beacon.set_supports_mic(true);
        beacon.set_ip_address(server_ip_);

        std::string payload;
        if (beacon.SerializeToString(&payload)) {
            sendto(sock, payload.data(), static_cast<int>(payload.size()), 0, (sockaddr*)&bcast_addr, sizeof(bcast_addr));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    closesocket(sock);
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(discovery_port_);
    bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    while (is_running_) {
        com::audiostream::pb::DiscoveryBeacon beacon;
        beacon.set_app_name("AudioStream");
        beacon.set_server_name(server_name_);
        beacon.set_port(server_tcp_port_);
        beacon.set_version("1.0.0");
        beacon.set_supports_opus(true);
        beacon.set_supports_mic(true);
        beacon.set_ip_address(server_ip_);

        std::string payload;
        if (beacon.SerializeToString(&payload)) {
            sendto(sock, payload.data(), payload.size(), 0, (sockaddr*)&bcast_addr, sizeof(bcast_addr));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    close(sock);
#endif
}

} // namespace audiostream::net
