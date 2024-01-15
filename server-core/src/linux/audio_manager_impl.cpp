/*
   Copyright 2022-2024 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifdef linux

#include "audio_manager.hpp"
#include "client.pb.h"
#include "network_manager.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spdlog/spdlog.h>

using namespace io::github::mkckr0::audio_share_app::pb;

struct roundtrip {
    struct pw_core* _core;
    int _sync;
    struct pw_main_loop* _loop;

    void operator()()
    {
        static const struct pw_core_events core_events = {
            .version = PW_VERSION_CORE_EVENTS,
            .done = [](void* data, uint32_t id, int seq) {
                struct roundtrip* d = (struct roundtrip*)data;
                if (id == PW_ID_CORE && seq == d->_sync)
                    pw_main_loop_quit(d->_loop);
            },
        };

        struct spa_hook core_listener;

        pw_core_add_listener(_core, &core_listener, &core_events, this);
        spdlog::trace("sync before: {}", _sync);
        _sync = pw_core_sync(_core, PW_ID_CORE, _sync);
        spdlog::trace("sync after: {}", _sync);
        pw_main_loop_run(_loop);

        spa_hook_remove(&core_listener);
    }
};

namespace detail {

audio_manager_impl::audio_manager_impl()
{
    pw_init(nullptr, nullptr);
    spdlog::info("pipewire header_version: {}, library_version: {}", pw_get_headers_version(), pw_get_library_version());

    _loop = pw_main_loop_new(nullptr);
    _context = pw_context_new(pw_main_loop_get_loop(_loop), nullptr, 0);
    _core = pw_context_connect(_context, nullptr, 0);
    _roundtrip = new roundtrip {
        ._core = _core,
        ._sync = 0,
        ._loop = _loop,
    };
}

audio_manager_impl::~audio_manager_impl()
{
    pw_core_disconnect(_core);
    pw_context_destroy(_context);
    pw_main_loop_destroy(_loop);
    pw_deinit();
    delete _roundtrip;
}

} // namespace detail

void audio_manager::do_loopback_recording(std::shared_ptr<network_manager> network_manager, const std::string& endpoint_id)
{
    spdlog::info("endpoint_id: {}", endpoint_id);
    endpoint_list_t endpoint_list;
    get_endpoint_list(endpoint_list);
    auto it = std::find_if(endpoint_list.begin(), endpoint_list.end(), [&](const endpoint_list_t::value_type& e) {
        return e.first == endpoint_id;
    });
    if (it != endpoint_list.end()) {
        spdlog::info("select audio endpoint: {}", it->second);
    }

    struct user_data_t {
        struct pw_stream* stream;
        std::shared_ptr<class network_manager> network_manager;
        std::shared_ptr<AudioFormat> format;
        int block_align;
    } user_data = {
        .stream = nullptr,
        .network_manager = network_manager,
        .format = _format,
        .block_align = 0,
    };

    static const struct pw_stream_events stream_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .param_changed = [](void* data, uint32_t id, const struct spa_pod* param) {
            struct user_data_t* user_data = (struct user_data_t*)data;

            if (param == nullptr || id != SPA_PARAM_Format) {
                return;
            }
            
            struct spa_audio_info audio_info;
            
            if (spa_format_parse(param, &audio_info.media_type, &audio_info.media_subtype) < 0) {
                return;
            }
            
            if (audio_info.media_type != SPA_MEDIA_TYPE_audio || audio_info.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
                return;
            }

            spa_format_audio_raw_parse(param, &audio_info.info.raw);

            switch (audio_info.info.raw.format)
            {
            case SPA_AUDIO_FORMAT_F32_LE:
            case SPA_AUDIO_FORMAT_F32_BE:
                user_data->format->set_format_tag(3);
                break;
            default:
                user_data->format->set_format_tag(1);
                break;
            }
            user_data->format->set_channels(audio_info.info.raw.channels);
            user_data->format->set_sample_rate(audio_info.info.raw.rate);
            switch (audio_info.info.raw.format)
            {
            case SPA_AUDIO_FORMAT_S8:
            case SPA_AUDIO_FORMAT_U8:
                user_data->format->set_bits_per_sample(8);
                break;
            case SPA_AUDIO_FORMAT_S16_LE:
            case SPA_AUDIO_FORMAT_S16_BE:
            case SPA_AUDIO_FORMAT_U16_LE:
            case SPA_AUDIO_FORMAT_U16_BE:
                user_data->format->set_bits_per_sample(16);
                break;
            case SPA_AUDIO_FORMAT_S32_LE:
            case SPA_AUDIO_FORMAT_S32_BE:
            case SPA_AUDIO_FORMAT_U32_LE:
            case SPA_AUDIO_FORMAT_U32_BE:
            case SPA_AUDIO_FORMAT_F32_LE:
            case SPA_AUDIO_FORMAT_F32_BE:
                user_data->format->set_bits_per_sample(32);
                break;
            default:
                user_data->format->set_bits_per_sample(0);
                break;
            }

            user_data->block_align = user_data->format->bits_per_sample() / 8 * user_data->format->channels();
            spdlog::info("block_align: {}", user_data->block_align);
            spdlog::info("AudioFormat:\n{}", user_data->format->DebugString());
        },
        .process = [](void* data) {
            struct user_data_t* user_data = (struct user_data_t*)data;
            struct pw_buffer *b;
            struct spa_buffer *buf;
    
            if ((b = pw_stream_dequeue_buffer(user_data->stream)) == nullptr) {
                pw_log_warn("out of buffers: %m");
                return;
            }
    
            buf = b->buffer;
            if (buf->datas[0].data == nullptr) {
                return;
            }

            auto begin = (const char*)buf->datas[0].data + buf->datas[0].chunk->offset;
            auto count = buf->datas[0].chunk->size;
            if (std::all_of(begin, begin + count, [](const char e) { return e == 0; })) {
                begin = nullptr;
                count = 0;
            }

            // if (spdlog::get_level() == spdlog::level::trace) {
            //     auto it = std::max_element(
            //         (float*)((char*)begin),
            //         (float*)((char*)begin + count)
            //     );
            //     spdlog::trace("offset: {}, size: {}, stride: {}, max: {}", buf->datas[0].chunk->offset, buf->datas[0].chunk->size, buf->datas[0].chunk->stride, *it);
            // }

            user_data->network_manager->broadcast_audio_data(begin, count, user_data->block_align);
    
            pw_stream_queue_buffer(user_data->stream, b); },
    };

    struct pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        nullptr);

    user_data.stream = pw_stream_new_simple(pw_main_loop_get_loop(_loop), "audio-share-server", props, &stream_events, &user_data);

    uint8_t buffer[1024];
    struct spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod* params[1];
    struct spa_audio_info_raw info = SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32_LE,
        .rate = 48000,
        .channels = 2);
    params[0] = spa_format_audio_raw_build(&pod_builder, SPA_PARAM_EnumFormat, &info);

    int ret = pw_stream_connect(user_data.stream, PW_DIRECTION_INPUT, std::stoi(endpoint_id),
        pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT
            | PW_STREAM_FLAG_MAP_BUFFERS
            | PW_STREAM_FLAG_RT_PROCESS),
        params, 1);

    pw_main_loop_run(_loop);

    pw_stream_destroy(user_data.stream);
}

int audio_manager::get_endpoint_list(endpoint_list_t& endpoint_list)
{
    struct user_data_t {
        endpoint_list_t* endpoint_list_ptr;
        int default_index;
        int default_prio;
    };

    static const struct pw_registry_events registry_events = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = [](void* object, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
            if (spa_streq(type, PW_TYPE_INTERFACE_Node)) {
                const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);

                if (!spa_streq(media_class, "Audio/Sink")) {
                    return;
                }

                auto user_data = (user_data_t*)object;
                const char* object_serial = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL);
                const char* nick_name = spa_dict_lookup(props, PW_KEY_NODE_NICK);
                const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
                const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
                const char* priority_session = spa_dict_lookup(props, PW_KEY_PRIORITY_SESSION);

                user_data->endpoint_list_ptr->push_back(std::make_pair<std::string, std::string>(
                    std::to_string(id),
                    nick_name ? nick_name : (description ? description : name)));

                int prio = priority_session ? std::atoi(priority_session) : -1;
                if (prio > user_data->default_prio) {
                    user_data->default_prio = prio;
                    user_data->default_index = user_data->endpoint_list_ptr->size() - 1;
                }

                spdlog::trace("object id: {}", id);
                const struct spa_dict_item* item;
                spa_dict_for_each(item, props)
                {
                    spdlog::trace("\t{}: \"{}\"", item->key, item->value);
                }
            }
        },
    };

    struct pw_registry* registry = pw_core_get_registry(_core, PW_VERSION_REGISTRY, 0);
    struct spa_hook registry_listener;
    struct user_data_t user_data = {
        .endpoint_list_ptr = &endpoint_list,
        .default_index = -1,
        .default_prio = -1,
    };
    pw_registry_add_listener(registry, &registry_listener, &registry_events, &user_data);
    (*_roundtrip)();

    pw_proxy_destroy((struct pw_proxy*)registry);
    return user_data.default_index;
}

std::string audio_manager::get_default_endpoint()
{
    endpoint_list_t endpoint_list;
    int index = get_endpoint_list(endpoint_list);
    return index >= 0 ? endpoint_list[index].first : "";
}

#endif // linux