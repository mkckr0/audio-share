#pragma once

#include <sdkddkver.h>
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

constexpr auto co_token = asio::as_tuple(asio::use_awaitable);

template<> struct fmt::formatter<asio::ip::tcp::endpoint> : fmt::ostream_formatter {};
template<> struct fmt::formatter<asio::ip::udp::endpoint> : fmt::ostream_formatter {};

enum class cmd_t : uint32_t {
    cmd_none = 0,
    cmd_get_format = 1,
    cmd_start_play = 2,
};

std::string wchars_to_mbs(const wchar_t* src);

std::string str_win_err(int err);

std::wstring wstr_win_err(int err);