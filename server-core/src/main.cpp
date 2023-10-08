#include <spdlog/spdlog.h>
#include <cxxopts.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    cxxopts::Options options("audio-share-server-cmd", "Example:\n  audio-share-server-cmd -l 192.168.3.2:65530 -e abcdefg\n");

    // clang-format off
    options.add_options()
        ("h,help", "Print usage")
        ("l,list-endpoints", "List all enpoints")
        ("b,bind", "The server bind address. The default port is 65530", cxxopts::value<std::string>(), "<host>[:<port>]")
        ("e,endpoint", "Specify the endpoint id. If not set, will use default", cxxopts::value<std::string>(), "[endpoint]")
        ;
    // clang-format on

    try
    {
        auto result = options.parse(argc, argv);
        if (result.arguments().size() == 1 && result.count("help"))
        {
            std::cout << options.help();
            return EXIT_SUCCESS;
        }

        if (result.arguments().size() == 1 && result.count("list-endpoints")) {
            return EXIT_SUCCESS;
        }

        std::cerr << options.help();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n' << options.help();
        return EXIT_FAILURE;
    }
}
