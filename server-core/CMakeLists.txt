cmake_minimum_required(VERSION 3.20.0)

project(audio-share)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (WIN32)
	add_compile_definitions(_UNICODE UNICODE)
endif()

set(
	lib_src_list
	"src/client.pb.cc"
	"src/audio_manager.cpp"
	"src/common.cpp"
	"src/network_manager.cpp"
)

add_executable(server-cmd
	${lib_src_list}
	"src/main.cpp"
)
set_target_properties(server-cmd PROPERTIES OUTPUT_NAME "audio-share-server-cmd")

find_package(asio CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE asio::asio)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE spdlog::spdlog)

find_package(Protobuf CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE protobuf::libprotobuf)

find_package(cxxopts CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE cxxopts::cxxopts)