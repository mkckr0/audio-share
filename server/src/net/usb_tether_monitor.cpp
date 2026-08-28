#include "usb_tether_monitor.h"
#include <iostream>
#include <chrono>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace audiostream::net {

UsbTetherMonitor::UsbTetherMonitor() = default;

UsbTetherMonitor::~UsbTetherMonitor() {
    stop();
}

InterfaceType UsbTetherMonitor::classify_ip(const std::string& ip) {
    if (ip.rfind("192.168.42.", 0) == 0 || ip.rfind("192.168.137.", 0) == 0) {
        return InterfaceType::USB_TETHERING;
    }
    if (ip.rfind("127.", 0) == 0 || ip == "::1") {
        return InterfaceType::LOOPBACK;
    }
    if (ip.rfind("192.168.", 0) == 0 || ip.rfind("10.", 0) == 0 || ip.rfind("172.", 0) == 0) {
        return InterfaceType::WIFI_LAN;
    }
    return InterfaceType::UNKNOWN;
}

std::vector<NetworkInterfaceInfo> UsbTetherMonitor::scan_interfaces() {
    std::vector<NetworkInterfaceInfo> list;

#if defined(_WIN32)
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    if (pAddresses) {
        DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
            if (pAddresses) {
                dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
            }
        }
        if (dwRetVal == NO_ERROR && pAddresses) {
            for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr; pCurr = pCurr->Next) {
                if (pCurr->OperStatus != IfOperStatusUp) continue;
                for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                        sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                        
                        NetworkInterfaceInfo info;
                        char nameBuf[256];
                        wcstombs(nameBuf, pCurr->FriendlyName, sizeof(nameBuf));
                        info.name = nameBuf;
                        info.ip_address = ipStr;
                        info.type = classify_ip(info.ip_address);
                        info.is_up = true;
                        list.push_back(info);
                    }
                }
            }
        }
        if (pAddresses) free(pAddresses);
    }
#else
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0 && ifaddr) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sa->sin_addr), ipStr, INET_ADDRSTRLEN);

            NetworkInterfaceInfo info;
            info.name = ifa->ifa_name;
            info.ip_address = ipStr;
            info.type = classify_ip(info.ip_address);
            info.is_up = true;
            list.push_back(info);
        }
        freeifaddrs(ifaddr);
    }
#endif

    return list;
}

bool UsbTetherMonitor::start(TetherStateChangeCallback callback) {
    if (is_monitoring_) stop();
    callback_ = callback;
    is_monitoring_ = true;
    monitor_thread_ = std::thread(&UsbTetherMonitor::monitor_thread_func, this);
    return true;
}

void UsbTetherMonitor::stop() {
    is_monitoring_ = false;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void UsbTetherMonitor::monitor_thread_func() {
    while (is_monitoring_) {
        auto ifaces = scan_interfaces();
        bool found_tether = false;
        std::string best_ip = "127.0.0.1";

        for (const auto& iface : ifaces) {
            if (iface.type == InterfaceType::USB_TETHERING) {
                found_tether = true;
                best_ip = iface.ip_address;
                break;
            } else if (iface.type == InterfaceType::WIFI_LAN && best_ip == "127.0.0.1") {
                best_ip = iface.ip_address;
            }
        }

        preferred_ip_ = best_ip;
        if (found_tether != is_tethered_) {
            is_tethered_ = found_tether;
            if (callback_) {
                callback_(found_tether, best_ip);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

} // namespace audiostream::net
