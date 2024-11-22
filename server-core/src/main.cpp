#include "config.h"
#include "audio_manager.hpp"
#include "network_manager.hpp"

#include <cxxopts.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

using string = std::string;

int main(int argc, char* argv[])
{
    auto default_address = network_manager::get_default_address();

    std::string help_string = "Example:\n";
    help_string += fmt::format("  {} -b\n", AUDIO_SHARE_BIN_NAME);
    help_string += fmt::format("  {} --bind={}\n", AUDIO_SHARE_BIN_NAME, default_address.empty() ? "192.168.3.2": default_address);
    help_string += fmt::format("  {} --bind={} --encoding=f32 --channels=2 --sample-rate=48000\n", AUDIO_SHARE_BIN_NAME, default_address.empty() ? "192.168.3.2": default_address);
    help_string += fmt::format("  {} -l\n", AUDIO_SHARE_BIN_NAME);
    help_string += fmt::format("  {} --list-encoding\n", AUDIO_SHARE_BIN_NAME);
    cxxopts::Options options(AUDIO_SHARE_BIN_NAME, help_string);

    // clang-format off
    options.add_options()
        ("h,help", "Print usage")
        ("l,list-endpoint", "List available endpoints")
        ("b,bind", "The server bind address. If not set, will use default", cxxopts::value<string>()->implicit_value(default_address), "[host][:<port>]")
        ("e,endpoint", "Specify the endpoint id. If not set or set \"default\", will use default", cxxopts::value<string>()->default_value("default"), "[endpoint]")
        ("encoding", "Specify the capture encoding. If not set or set \"default\", will use default", cxxopts::value<audio_manager::encoding_t>()->default_value("default"), "[encoding]")
        ("list-encoding", "List available encoding")
        ("channels", "Specify the capture channels. If not set or set \"0\", will use default", cxxopts::value<int>()->default_value("0"), "[channels]")
        ("sample-rate", "Specify the capture sample rate(Hz). If not set or set \"0\", will use default. The common values are 44100, 48000, etc.", cxxopts::value<int>()->default_value("0"), "[sample_rate]")
        ("V,verbose", "Set log level to \"trace\"")
        ("v,version", "Show version")
        ;
    // clang-format on

    try {
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help();
            return EXIT_SUCCESS;
        }

        if (result.count("version")) {
            fmt::println("{}\nversion: {}\nurl: {}\n", AUDIO_SHARE_BIN_NAME, AUDIO_SHARE_VERSION, AUDIO_SHARE_HOMEPAGE);
            return EXIT_SUCCESS;
        }

        if (result.count("verbose")) {
            spdlog::set_level(spdlog::level::trace);
        }

        if (result.count("list-endpoint")) {
            auto audio_manager = std::make_shared<class audio_manager>();
            auto endpoint_list = audio_manager->get_endpoint_list();
            auto default_endpoint = audio_manager->get_default_endpoint();
            auto s = fmt::format("endpoint list:\n");
            for (auto&& [id, name] : endpoint_list) {
                s += fmt::format("\t{} id: {:4} name: {}\n", (id == default_endpoint ? '*' : ' '), id, name);
            }
            s += fmt::format("total: {}\n", endpoint_list.size());
            std::cout << s;
            return EXIT_SUCCESS;
        }

        if (result.count("list-encoding")) {
            std::vector<std::pair<string, string>> array = {
                { "default", "Default encoding" },
                { "f32", "32 bit floating-point PCM" },
                { "s8", "8 bit integer PCM" },
                { "s16", "16 bit integer PCM" },
                { "s24", "24 bit integer PCM" },
                { "s32", "32 bit integer PCM" },
            };
            fmt::println("encoding list:");
            for(auto&& e : array) {
                fmt::println("\t{}\t\t{}", e.first, e.second);
            }
            return EXIT_SUCCESS;
        }

        if (result.count("bind")) {
            auto s = result["bind"].as<string>();
            size_t pos = s.find(':');
            string host = s.substr(0, pos);
            uint16_t port;
            if (pos == string::npos) {
                port = 65530;
            } else {
                port = (uint16_t)std::stoi(s.substr(pos + 1));
            }

            auto audio_manager = std::make_shared<class audio_manager>();

            audio_manager::capture_config capture_config;

            capture_config.endpoint_id = result["endpoint"].as<string>();
            capture_config.encoding = result["encoding"].as<audio_manager::encoding_t>();
            capture_config.channels = result["channels"].as<int>();
            capture_config.sample_rate = result["sample-rate"].as<int>();

            auto network_manager = std::make_shared<class network_manager>(audio_manager);

            network_manager->start_server(host, port, capture_config);
            network_manager->wait_server();

            return EXIT_SUCCESS;
        }

        // no arg
        std::cerr << options.help();
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << e.what() << '\n'
                  << options.help();
        return EXIT_FAILURE;
    } catch (const std::exception& ec) {
        std::cerr << ec.what() << '\n';
        return EXIT_FAILURE;
    }
}
