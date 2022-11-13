#include "network_manager.hpp"
#include "common.hpp"
#include "audio_manager.hpp"

#include <iostream>

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

namespace ip = asio::ip;

std::vector<std::wstring> network_manager::get_local_addresss()
{
    std::vector<std::wstring> address_list;

    ULONG family = AF_INET;
    ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES;

    ULONG size = 0;
    GetAdaptersAddresses(family, flags, NULL, NULL, &size);
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

    auto ret = GetAdaptersAddresses(family, flags, NULL, pAddresses, &size);
    if (ret == ERROR_SUCCESS) {
        for (auto pCurrentAddress = pAddresses; pCurrentAddress; pCurrentAddress = pCurrentAddress->Next) {
            if (pCurrentAddress->OperStatus != IfOperStatusUp || pCurrentAddress->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                continue;
            }

            for (auto pUnicast = pCurrentAddress->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                auto sockaddr = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                wchar_t buf[20]{};
                if (InetNtop(AF_INET, &sockaddr->sin_addr, buf, std::size(buf))) {
                    address_list.push_back(buf);
                }
            }
        }
    }

    free(pAddresses);

    std::sort(address_list.begin(), address_list.end());
    return address_list;
}

asio::awaitable<void> network_manager::read_loop(std::shared_ptr<ip::tcp::socket> peer)
{
    while (true) {
        cmd_t cmd = cmd_t::cmd_none;
        auto [ec, _] = co_await asio::async_read(*peer, asio::buffer(&cmd, sizeof(cmd)), co_token);
        if (ec) {
            audio_manager->remove_playing_peer(peer);
            peer->shutdown(ip::tcp::socket::shutdown_both);
            spdlog::info("{} {}", __func__, ec.message());
            co_return;
        }

        spdlog::info("cmd {}", (uint32_t)cmd);

        if (cmd == cmd_t::cmd_get_format) {
            auto& format = audio_manager->get_format();
            uint32_t size = (uint32_t)format.size();
            co_await asio::async_write(*peer, asio::buffer(&cmd, sizeof(cmd)), co_token);
            co_await asio::async_write(*peer, asio::buffer(&size, sizeof(size)), co_token);
            co_await asio::async_write(*peer, asio::buffer(format), co_token);
        }
        else if (cmd == cmd_t::cmd_start_play) {
            audio_manager->add_playing_peer(peer);
        }
        else {
            spdlog::error("{} error cmd", __func__);
            audio_manager->remove_playing_peer(peer);
            peer->shutdown(ip::tcp::socket::shutdown_both);
            co_return;
        }
    }
}

asio::awaitable<void> network_manager::accept_loop(ip::tcp::acceptor acceptor)
{
    while (true) {
        auto peer = std::make_shared<ip::tcp::socket>(acceptor.get_executor());
        auto [ec] = co_await acceptor.async_accept(*peer, co_token);
        if (ec) {
            spdlog::error("{} {}", __func__, ec.message());
            co_return;
        }
        spdlog::info("{} {}", __func__, peer->remote_endpoint());
        peer->set_option(ip::tcp::socket::keep_alive(true), ec);
        if (ec) {
            spdlog::info("{} {}", __func__, ec.message());
        }
        peer->set_option(ip::tcp::no_delay(true), ec);
        if (ec) {
            spdlog::info("{} {}", __func__, ec.message());
        }

        asio::co_spawn(acceptor.get_executor(), read_loop(peer), asio::detached);
    }
}

void network_manager::start_server(const std::string& host, const uint16_t port, const std::wstring& endpoint_id)
{
    ioc = std::make_shared<asio::io_context>();

    std::promise<void> promise;
    auto future = promise.get_future();
    net_thread = std::thread([=, promise = std::move(promise)]() mutable {

        try {
            ip::tcp::endpoint endpoint{ ip::make_address(host), port };

            ip::tcp::acceptor acceptor(*ioc, endpoint.protocol());
            acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
            acceptor.bind(endpoint);

            audio_manager = std::make_unique<class audio_manager>();
            asio::co_spawn(*ioc, audio_manager->do_loopback_recording(*ioc, endpoint_id), asio::detached);
            ioc->run_one();

            acceptor.listen();

            // start success
            spdlog::info("listen success on {}", endpoint);
            promise.set_value();

            asio::co_spawn(*ioc, accept_loop(std::move(acceptor)), asio::detached);
        }
        catch (...) {
            promise.set_exception(std::current_exception());
            return;
        }

        ioc->run();
    });

    try {
        future.get();
    }
    catch (...) {
        net_thread.join();
        audio_manager = nullptr;
        ioc = nullptr;
        throw;
    }
}

void network_manager::stop_server()
{
    ioc->stop();
    net_thread.join();
    audio_manager = nullptr;
    ioc = nullptr;
}
