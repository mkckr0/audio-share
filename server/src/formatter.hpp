#ifndef AUDIOSTREAM_FORMATTER_HPP
#define AUDIOSTREAM_FORMATTER_HPP

#include <fmt/ostream.h>
#include "pre_asio.hpp"
#include <asio.hpp>

template<> struct fmt::formatter<asio::ip::tcp::endpoint> : fmt::ostream_formatter {};
template<> struct fmt::formatter<asio::ip::udp::endpoint> : fmt::ostream_formatter {};
template<> struct fmt::formatter<asio::error_code> : fmt::formatter<std::string_view> {
    auto format(asio::error_code& ec, format_context& ctx) const {
        return formatter<std::string_view>::format(ec.message(), ctx);
    }
};

#endif // AUDIOSTREAM_FORMATTER_HPP
