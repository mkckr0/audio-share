#include "audio_manager.hpp"

audio_manager::audio_manager()
{
    _format = std::make_unique<AudioFormat>();
}

audio_manager::~audio_manager() = default;

void audio_manager::start_loopback_recording(std::shared_ptr<network_manager> network_manager, const capture_config& config)
{
    _stopped = false;
    _record_thread = std::thread([network_manager = network_manager, config = config, self = shared_from_this()] {
        self->do_loopback_recording(network_manager, config);
    });
}

void audio_manager::stop()
{
    _stopped = true;
    _record_thread.join();
}

std::string audio_manager::get_format_binary()
{
    return _format->SerializeAsString();
}