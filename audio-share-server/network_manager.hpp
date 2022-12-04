#pragma once

#include <sdkddkver.h>

#include <memory>
#include <vector>
#include <string>
#include <asio.hpp>

class audio_manager;

class network_manager
{
    std::unique_ptr<class audio_manager> audio_manager;

public:

    network_manager() {}

    static std::vector<std::wstring> get_local_addresss();

    asio::awaitable<void> read_loop(std::shared_ptr<asio::ip::tcp::socket> peer);

    asio::awaitable<void> accept_tcp_loop(asio::ip::tcp::acceptor acceptor);
    
    asio::awaitable<void> accept_udp_loop(std::shared_ptr<asio::ip::udp::socket> acceptor);

    void start_server(const std::string& host, const uint16_t port, const std::wstring& endpoint_id);

    void stop_server();

private:
    std::shared_ptr<asio::io_context> ioc;
    std::thread net_thread;
};