#include "audio_manager.hpp"

void audio_manager::start_loopback_recording(std::shared_ptr<network_manager> network_manager, const std::string& endpoint_id)
{
    _record_thread = std::thread([network_manager = network_manager, endpoint_id = endpoint_id, self = shared_from_this()] {
        self->do_loopback_recording(network_manager, endpoint_id);
    });
}