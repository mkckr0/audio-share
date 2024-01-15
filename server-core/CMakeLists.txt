cmake_minimum_required(VERSION 3.20.0)

project(audio-share
	VERSION 0.0.17
	HOMEPAGE_URL "https://github.com/mkckr0/audio-share"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(AUDIO_SHARE_BIN_NAME "as-cmd")
configure_file(src/config.h.in config.h)

if(WIN32)
	add_compile_definitions(_UNICODE UNICODE)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(PLATFORM_NAME "win32")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(PLATFORM_NAME "linux")
endif()

message(STATUS "PLATFORMA_NAME ${PLATFORM_NAME}")

find_package(Protobuf CONFIG REQUIRED)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../protos/client.proto)

include_directories("src")
set(
	lib_src_list
	"src/network_manager.cpp"
	"src/audio_manager.cpp"
	"src/${PLATFORM_NAME}/audio_manager_impl.cpp"
	${PROTO_SRCS}
)

add_executable(server-cmd
	${lib_src_list}
	"src/main.cpp"
)
set_target_properties(server-cmd PROPERTIES OUTPUT_NAME ${AUDIO_SHARE_BIN_NAME})
if(UNIX)
	target_link_options(server-cmd PRIVATE "-static-libstdc++")
endif(UNIX)

find_package(asio CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE asio::asio)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE spdlog::spdlog)

find_package(Protobuf CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE protobuf::libprotobuf)

find_package(cxxopts CONFIG REQUIRED)
target_link_libraries(server-cmd PRIVATE cxxopts::cxxopts)

if(${PLATFORM_NAME} STREQUAL "linux")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(pipewire REQUIRED IMPORTED_TARGET libpipewire-0.3)
	target_link_libraries(server-cmd PRIVATE PkgConfig::pipewire)
endif()

install(TARGETS server-cmd)