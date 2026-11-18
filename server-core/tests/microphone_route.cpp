#include "win32/microphone_route.hpp"

#include <cstdlib>
#include <iostream>

static void check(bool pass, const char* message)
{
    if (!pass) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace std::chrono_literals;
    using mode = playback_source::mode;
    const playback_routing::clock::time_point start{};
    const uint64_t mic = (1ULL << 32) | 101;
    const uint64_t released = 2ULL << 32;
    const uint64_t replacement_mic = (3ULL << 32) | 202;
    playback_routing duplex(true, false, start);
    check(duplex.next(0, start).type == mode::endpoint, "physical endpoint should start immediately");
    check(duplex.next(mic, start).pid == 101, "receiver was not excluded");
    check(duplex.next(released, start + 20ms).pid == 101, "release exposed the driver's buffered microphone");
    check(duplex.next(released, start + 800ms).pid == 101, "largest cable pipe has not drained");
    check(duplex.next(replacement_mic, start + 850ms).type == mode::pause, "two receiver tails cannot share one filter");
    check(duplex.next(replacement_mic, start + 1021ms).pid == 202, "new receiver failed to take over");

    playback_routing pinned(false, true, start);
    check(pinned.next(0, start).type == mode::pause, "cold cable may contain a previous receiver's tail");
    check(pinned.next(0, start + 1s).type == mode::endpoint, "cold cable never started");
    check(pinned.next(mic, start + 2s).type == mode::pause, "pinned mixed endpoint must pause");
    check(pinned.next(released, start + 3s).type == mode::pause, "pinned endpoint exposed its release tail");
    check(pinned.next(released, start + 4s).type == mode::endpoint, "pinned endpoint failed to resume");
    check(pinned.next(4ULL << 32, start + 5s).type == mode::pause, "a short-lived receiver was missed between packets");
    // Simulate a receiver starting and restarting while two speakers already
    // watch different endpoints. No physical audio hardware is required.
    const auto id = "test-" + std::to_string(GetCurrentProcessId());
    microphone_route speaker(id);
    microphone_route elsewhere(id + "-other");
    const auto idle = speaker.snapshot();
    check(idle == 0 && elsewhere.snapshot() == 0, "new routes must be idle");

    wil::unique_handle mapping(OpenFileMappingW(FILE_MAP_WRITE, FALSE, microphone_route::name(id).c_str()));
    check(static_cast<bool>(mapping), "cannot open speaker route from receiver");
    wil::unique_mapview_ptr<LONG64> state(static_cast<LONG64*>(
        MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, sizeof(LONG64))));
    check(static_cast<bool>(state), "cannot map speaker route from receiver");
    InterlockedExchange64(state.get(), (1LL << 32) | GetCurrentProcessId());
    const auto active = speaker.snapshot();
    check(DWORD(active) == GetCurrentProcessId(), "receiver PID missing");
    check(elsewhere.snapshot() == 0, "independent endpoint acquired the receiver route");

    const uint64_t replacement = (2ULL << 32) | GetCurrentProcessId();
    InterlockedExchange64(state.get(), replacement);
    speaker.receiver_exited(active);
    check(speaker.snapshot() == replacement, "late cleanup displaced a new receiver");
    speaker.receiver_exited(replacement);
    check(speaker.snapshot() != idle, "reader missed a short-lived receiver");
    check(DWORD(speaker.snapshot()) == 0, "receiver PID did not clear");
    check(microphone_route::name("test") == L"Local\\AudioShare.Microphone.v1.f9e6e6ef197c2b25",
          "route name differs from the Go receiver");
    return 0;
}
