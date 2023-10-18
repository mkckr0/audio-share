#include <cxxopts.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

#include "audio_manager.hpp"
#include "network_manager.hpp"

int main(int argc, char* argv[])
{
    cxxopts::Options options("audio-share-server-cmd", "Example:\n  audio-share-server-cmd -l 192.168.3.2:65530 -e 48\n");

    // clang-format off
    options.add_options()
        ("h,help", "Print usage")
        ("l,list", "List all enpoints")
        ("b,bind", "The server bind address. The default port is 65530", cxxopts::value<std::string>(), "<host>[:<port>]")
        ("e,endpoint", "Specify the endpoint id. If not set, will use default", cxxopts::value<std::string>(), "[endpoint]")
        ("v,verbose", "Set log level to \"trace\"")
        ;
    // clang-format on

    try {
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help();
            return EXIT_SUCCESS;
        }

        if (result.count("verbose")) {
            spdlog::set_level(spdlog::level::trace);
        }

        if (result.count("list")) {
            class audio_manager audio_manager;
            audio_manager::endpoint_list_t endpoint_list;
            int default_index = audio_manager.get_endpoint_list(endpoint_list);
            spdlog::info("endpoint_list: {}", endpoint_list.size());
            for (int i = 0; i < endpoint_list.size(); ++i) {
                spdlog::info("\t{} id: {:4} name: {}", (default_index == i ? '*' : ' '), endpoint_list[i].first, endpoint_list[i].second);
            }
            return EXIT_SUCCESS;
        }

        if (result.count("bind")) {
            auto s = result["bind"].as<std::string>();
            if (s.empty()) {
                std::cerr << options.help();
                return EXIT_FAILURE;
            }

            size_t pos = s.find(':');
            std::string host = s.substr(0, pos);
            uint16_t port;
            if (pos == s.npos) {
                port = 65530;
            } else {
                port = (uint16_t)std::stoi(s.substr(pos + 1));
            }

            auto audio_manager = std::make_shared<class audio_manager>();

            std::string endpoint_id;
            if (result.count("endpoint")) {
                endpoint_id = result["endpoint"].as<std::string>();
            } else {
                endpoint_id = audio_manager->get_default_endpoint();
            }
            if (endpoint_id.empty()) {
                std::cerr << "endpoint_id is empty\n";
                return EXIT_FAILURE;
            }

            auto network_manager = std::make_shared<class network_manager>(audio_manager);
            network_manager->start_server(host, port, endpoint_id);
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
