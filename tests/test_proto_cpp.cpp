#include <iostream>
#include <cassert>
#include <string>
#include <vector>

// Include generated protobuf headers
#include "audiostream.pb.h"

int main() {
    std::cout << "=== Running C++ Native Protobuf Verification ===" << std::endl;

    // 1. Test AudioFormat
    {
        com::audiostream::pb::AudioFormat fmt;
        fmt.set_encoding(com::audiostream::pb::ENCODING_PCM_FLOAT);
        fmt.set_channels(2);
        fmt.set_sample_rate(48000);

        std::string serialized;
        assert(fmt.SerializeToString(&serialized));

        com::audiostream::pb::AudioFormat deserialized;
        assert(deserialized.ParseFromString(serialized));
        assert(deserialized.encoding() == com::audiostream::pb::ENCODING_PCM_FLOAT);
        assert(deserialized.channels() == 2);
        assert(deserialized.sample_rate() == 48000);
        std::cout << "[PASS] C++ AudioFormat serialization roundtrip" << std::endl;
    }

    // 2. Test StreamConfig
    {
        com::audiostream::pb::StreamConfig cfg;
        auto* fmt = cfg.mutable_format();
        fmt->set_encoding(com::audiostream::pb::ENCODING_PCM_16BIT);
        fmt->set_channels(2);
        fmt->set_sample_rate(44100);
        cfg.set_codec(com::audiostream::pb::CODEC_OPUS);
        cfg.set_opus_bitrate(128000);
        cfg.set_opus_frame_size_ms(20);
        cfg.set_target_latency_ms(40);

        std::string serialized;
        assert(cfg.SerializeToString(&serialized));

        com::audiostream::pb::StreamConfig deser;
        assert(deser.ParseFromString(serialized));
        assert(deser.codec() == com::audiostream::pb::CODEC_OPUS);
        assert(deser.opus_bitrate() == 128000);
        assert(deser.opus_frame_size_ms() == 20);
        assert(deser.target_latency_ms() == 40);
        assert(deser.format().sample_rate() == 44100);
        std::cout << "[PASS] C++ StreamConfig serialization roundtrip" << std::endl;
    }

    // 3. Test MicControl
    {
        com::audiostream::pb::MicControl mic;
        mic.set_start(true);
        mic.set_mic_source(1);
        auto* cfg = mic.mutable_config();
        cfg->set_codec(com::audiostream::pb::CODEC_OPUS);
        cfg->set_opus_bitrate(64000);

        std::string serialized;
        assert(mic.SerializeToString(&serialized));

        com::audiostream::pb::MicControl deser;
        assert(deser.ParseFromString(serialized));
        assert(deser.start() == true);
        assert(deser.mic_source() == 1);
        assert(deser.config().opus_bitrate() == 64000);
        std::cout << "[PASS] C++ MicControl serialization roundtrip" << std::endl;
    }

    // 4. Test DiscoveryBeacon
    {
        com::audiostream::pb::DiscoveryBeacon beacon;
        beacon.set_app_name("AudioStream");
        beacon.set_server_name("DESKTOP-SERVER");
        beacon.set_port(59200);
        beacon.set_version("1.0.0");
        beacon.set_supports_opus(true);
        beacon.set_supports_mic(true);
        beacon.set_ip_address("192.168.42.1");

        std::string serialized;
        assert(beacon.SerializeToString(&serialized));

        com::audiostream::pb::DiscoveryBeacon deser;
        assert(deser.ParseFromString(serialized));
        assert(deser.app_name() == "AudioStream");
        assert(deser.server_name() == "DESKTOP-SERVER");
        assert(deser.port() == 59200);
        assert(deser.version() == "1.0.0");
        assert(deser.supports_opus() == true);
        assert(deser.supports_mic() == true);
        assert(deser.ip_address() == "192.168.42.1");
        std::cout << "[PASS] C++ DiscoveryBeacon serialization roundtrip" << std::endl;
    }

    // 5. Test Edge Cases: Max values and negative values
    {
        com::audiostream::pb::StreamConfig edge_cfg;
        edge_cfg.set_opus_bitrate(-1);
        edge_cfg.set_opus_frame_size_ms(2147483647);
        edge_cfg.set_target_latency_ms(-50);

        std::string serialized;
        assert(edge_cfg.SerializeToString(&serialized));

        com::audiostream::pb::StreamConfig deser;
        assert(deser.ParseFromString(serialized));
        assert(deser.opus_bitrate() == -1);
        assert(deser.opus_frame_size_ms() == 2147483647);
        assert(deser.target_latency_ms() == -50);
        std::cout << "[PASS] C++ Edge case negative/boundary values" << std::endl;
    }

    std::cout << "=== All C++ Native Protobuf Verification Tests Passed! ===" << std::endl;
    return 0;
}
