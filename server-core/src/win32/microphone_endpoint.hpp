#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include <cwchar>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace detail {

inline bool can_render_microphone(const wchar_t* selected_mmdevice_id)
{
    if (!selected_mmdevice_id || !*selected_mmdevice_id) return true;
    const UINT count = waveOutGetNumDevs();
    if (!count) return true;

    for (UINT index = 0; index < count; ++index) {
        WAVEOUTCAPSW caps{};
        if (waveOutGetDevCapsW(index, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            // Match selectOutput in server-mic/device.go, including WinMM truncation.
            const int length = static_cast<int>(wcsnlen(caps.szPname, MAXPNAMELEN));
            if (!length) continue;
            const int pos = FindStringOrdinal(FIND_FROMSTART, caps.szPname, length, L"vb-audio virtual", -1, TRUE);
            if (pos < 0 && GetLastError() == ERROR_SUCCESS) continue;
        }

        // This message takes a numeric waveOut ID cast to HWAVEOUT.
        auto device = reinterpret_cast<HWAVEOUT>(static_cast<UINT_PTR>(index));
        DWORD_PTR bytes = 0;
        auto result = waveOutMessage(device, DRV_QUERYFUNCTIONINSTANCEIDSIZE, reinterpret_cast<DWORD_PTR>(&bytes), 0);
        if (result != MMSYSERR_NOERROR || bytes < 2 * sizeof(wchar_t) || bytes % sizeof(wchar_t) || bytes > 65536)
            return true;
        std::vector<wchar_t> id(bytes / sizeof(wchar_t), L'\uffff');
        result = waveOutMessage(device, DRV_QUERYFUNCTIONINSTANCEID, reinterpret_cast<DWORD_PTR>(id.data()), bytes);
        if (result != MMSYSERR_NOERROR || id.back() != L'\0' || std::wcslen(id.data()) + 1 != id.size())
            return true;
        const int match = CompareStringOrdinal(id.data(), -1, selected_mmdevice_id, -1, TRUE);
        if (!match || match == CSTR_EQUAL) return true;
    }
    return false;
}

} // namespace detail
