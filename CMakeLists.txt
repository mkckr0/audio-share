cmake_minimum_required(VERSION 3.20.0)

project(AudioStream
    VERSION 1.0.0
    LANGUAGES C CXX
    DESCRIPTION "AudioStream - High-performance bidirectional PC/Android audio streaming"
)

# Output directory for release binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")

# Subdirectories
add_subdirectory(server)
