#pragma once

#include <windows.h>
#include <wil/resource.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

// A sink publishes its PID before the first waveOutWrite and clears it after
// waveOutClose. Keeping the mapping open lets a reader detect a complete
// start/stop cycle between two packets. See server-mic/route_windows.go.
class microphone_route {
public:
    static std::wstring name(const std::string& endpoint_utf8)
    {
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : endpoint_utf8) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        std::wostringstream result;
        result << L"Local\\AudioShare.Microphone.v1." << std::hex << std::setfill(L'0') << std::setw(16) << hash;
        return result.str();
    }

    explicit microphone_route(const std::string& endpoint_utf8) : object_name(name(endpoint_utf8))
    {
        mapping.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                         sizeof(LONG64), object_name.c_str()));
        THROW_LAST_ERROR_IF_NULL(mapping);
        state.reset(static_cast<LONG64*>(MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, sizeof(LONG64))));
        THROW_LAST_ERROR_IF_NULL(state);
    }

    uint64_t snapshot() const
    {
        return static_cast<uint64_t>(InterlockedCompareExchange64(state.get(), 0, 0));
    }

    // A crashed receiver cannot release the route. A reconnecting receiver
    // may already have claimed it, so clear only the epoch we observed.
    void receiver_exited(uint64_t epoch)
    {
        const uint64_t idle = ((epoch >> 32) + 1) << 32;
        InterlockedCompareExchange64(state.get(), static_cast<LONG64>(idle), static_cast<LONG64>(epoch));
    }

    bool writer_gone() const
    {
        // Used only if the receiver's process handle cannot be opened.
        wil::unique_handle writer(OpenMutexW(SYNCHRONIZE, FALSE, (object_name + L".writer").c_str()));
        return !writer && GetLastError() == ERROR_FILE_NOT_FOUND;
    }

private:
    std::wstring object_name;
    wil::unique_handle mapping;
    wil::unique_mapview_ptr<LONG64> state;
};

struct playback_source {
    enum class mode { endpoint, exclude_receiver, pause };
    mode type;
    DWORD pid = 0;
    bool operator==(const playback_source&) const = default;
};

class playback_routing {
public:
    using clock = std::chrono::steady_clock;

    playback_routing(bool allow_system_mix, bool cold_cable, clock::time_point now)
        : allow_mix(allow_system_mix), drain_until(cold_cable ? now + drain_time : now)
    {
    }

    playback_source next(uint64_t epoch, clock::time_point now)
    {
        const DWORD active = static_cast<DWORD>(epoch);
        if (previous && epoch != *previous) {
            const DWORD old_pid = static_cast<DWORD>(*previous);
            const DWORD generation = static_cast<DWORD>(epoch >> 32);
            const DWORD expected = static_cast<DWORD>(*previous >> 32) + 1;
            if (generation != expected) {
                // A whole session may have elapsed between reads.
                remember_tail(0, now);
            } else if (old_pid && old_pid != active) {
                remember_tail(old_pid, now);
            }
        }
        previous = epoch;
        DWORD exclude = active;
        if (now < drain_until) {
            // A single process filter cannot cover two distinct receivers.
            if (!tail_pid || (active && active != tail_pid))
                return { playback_source::mode::pause };
            exclude = tail_pid;
        }
        if (!exclude) return { playback_source::mode::endpoint };
        if (!allow_mix) return { playback_source::mode::pause };
        return { playback_source::mode::exclude_receiver, exclude };
    }

private:
    // VB-CABLE can retain 32768 samples at 44.1 kHz (743 ms), plus the
    // render/loopback engine buffers. waveOutClose does not drain that pipe.
    static constexpr auto drain_time = std::chrono::seconds(1);
    bool allow_mix;
    clock::time_point drain_until;
    DWORD tail_pid = 0;
    std::optional<uint64_t> previous;

    void remember_tail(DWORD pid, clock::time_point now)
    {
        if (now >= drain_until || tail_pid == pid) tail_pid = pid;
        else tail_pid = 0;
        drain_until = now + drain_time;
    }
};
