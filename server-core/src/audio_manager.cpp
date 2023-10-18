#include "audio_manager.hpp"

audio_manager::audio_manager()
{
    _format = std::make_shared<AudioFormat>();
}

audio_manager::~audio_manager()
{
    
}

void audio_manager::start_loopback_recording(std::shared_ptr<network_manager> network_manager, const std::string& endpoint_id)
{
    _stoppped = false;
    _record_thread = std::thread([network_manager = network_manager, endpoint_id = endpoint_id, self = shared_from_this()] {
        self->do_loopback_recording(network_manager, endpoint_id);
    });
}

void audio_manager::stop()
{
    _stoppped = true;
    _record_thread.join();
}

std::string audio_manager::get_format_binary()
{
    return _format->SerializeAsString();
}