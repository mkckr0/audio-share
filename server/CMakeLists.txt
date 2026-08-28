cmake_minimum_required(VERSION 3.20.0)

project(AudioStream
    VERSION 1.0.0
    LANGUAGES C CXX
    DESCRIPTION "AudioStream - High-performance bidirectional PC/Android audio streaming"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Debranded Configuration Definitions
set(AUDIOSTREAM_BIN_NAME "AudioStream")
configure_file(src/config.h.in config.h)

if(WIN32 OR MINGW)
    add_compile_definitions(
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
    )
endif()

# Protobuf Code Generation
find_package(Protobuf REQUIRED)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../protos/audiostream.proto)

# Include Paths
include_directories("src")
include_directories("vendor")
include_directories("vendor/asio/include")
include_directories("vendor/spdlog/include")
include_directories("vendor/opus/include")
include_directories("vendor/imgui")
include_directories("vendor/imgui_backends")

# Server Sources
set(SERVER_SOURCES
    src/main.cpp
    src/audio/wasapi_capture.cpp
    src/audio/wasapi_render.cpp
    src/audio/virtual_mic_monitor.cpp
    src/codec/opus_encoder.cpp
    src/codec/opus_decoder.cpp
    src/codec/jitter_buffer.cpp
    src/net/network_manager.cpp
    src/net/usb_tether_monitor.cpp
    src/net/discovery_beacon.cpp
    src/ui/imgui_ui.cpp
    src/ui/d3d11_backend.cpp
    src/ui/tray_icon.cpp
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui_backends/imgui_impl_win32.cpp
    vendor/imgui_backends/imgui_impl_dx11.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

# Executable Target
if(WIN32 OR MINGW)
    add_executable(AudioStream WIN32 ${SERVER_SOURCES})
else()
    add_executable(AudioStream ${SERVER_SOURCES})
endif()

# Set Output Binary Properties
set_target_properties(AudioStream PROPERTIES
    OUTPUT_NAME ${AUDIOSTREAM_BIN_NAME}
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist"
)

# Link Libraries
if(WIN32 OR MINGW)
    target_link_libraries(AudioStream PRIVATE
        protobuf::libprotobuf
        opus
        ole32
        oleaut32
        mmdevapi
        avrt
        d3d11
        dxgi
        d3dcompiler
        dwmapi
        uxtheme
        shell32
        user32
        gdi32
        iphlpapi
        ws2_32
        mswsock
        pthread
    )
    if(MINGW)
        target_link_options(AudioStream PRIVATE
            "-static"
            "-static-libgcc"
            "-static-libstdc++"
            "-mwindows"
        )
    endif()
else()
    target_link_libraries(AudioStream PRIVATE
        protobuf::libprotobuf
        opus
        pthread
    )
endif()

install(TARGETS AudioStream RUNTIME DESTINATION bin)
