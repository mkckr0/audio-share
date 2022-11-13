#pragma once

#include <sdkddkver.h>

#include <set>
#include <map>
#include <vector>
#include <asio.hpp>
#include <mmreg.h>
#include <mmdeviceapi.h>

class audio_manager
{
public:
    audio_manager();
    ~audio_manager();

    asio::awaitable<void> do_loopback_recording(asio::io_context& ioc, const std::wstring& endpoint_id);

    void add_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer);
    void remove_playing_peer(std::shared_ptr<asio::ip::tcp::socket> peer);

    const std::vector<uint8_t>& get_format() const;

    static std::map<std::wstring, std::wstring> get_audio_endpoint_map();
    static std::wstring get_default_endpoint();

private:
    void set_format(WAVEFORMATEX* format);
    void broadcast_audio_data(const char* data, size_t count);

    std::set<std::shared_ptr<asio::ip::tcp::socket>> _playing_peer_list;
    std::vector<uint8_t> _format;

    constexpr static int _endpoint_state_mask = DEVICE_STATE_ACTIVE;
};

