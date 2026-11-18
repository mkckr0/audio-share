# Audio Share microphone receiver for Windows

This repository-owned receiver sends the phone microphone to an explicitly
selected VB-CABLE playback endpoint. Windows applications select **CABLE Output**
as their microphone. The separate Windows speaker server uses the receiver's
published playback endpoint and process ID to prevent microphone feedback.
Update both binaries for [simultaneous microphone and speaker use](../docs/microphone.md#sharing-a-playback-endpoint).

Build and verify with Go 1.23 or later on Windows; there are no external packages
or C compiler requirements:

```powershell
go test ./...
go vet ./...
go build -trimpath -o audio-share-mic.exe .
```

Start the receiver and forward the Android localhost connection over USB:

```powershell
.\audio-share-mic.exe --list-devices
adb reverse tcp:65529 tcp:65529
.\audio-share-mic.exe --status-file .\mic-status.json --stop-file .\mic-stop
```

Remove an old `mic-stop` sentinel before starting. Creating it stops the process
within one polling interval (250 ms), closes the connection, resets queued audio,
and releases the output. Ctrl+C also stops it. The launcher can use the stop-file
option when running the process hidden.

The defaults are `--listen 127.0.0.1:65529` and `--device "CABLE Input"`.
Only explicit loopback IPs are accepted. Device names use a case-insensitive
substring match; zero or multiple matches are errors. There is no default-device
or speaker fallback. Even an explicit `--device` selection is rejected unless
its WinMM name identifies `VB-Audio Virtual`; physical speakers are not accepted.
WinMM truncates display names to 31 UTF-16 characters, so
use the names printed by `--list-devices`. The installed VB-CABLE Pack46 exposes
its stereo playback endpoint to WinMM as `CABLE Input (VB-Audio Virtual C`.

`--status-file` writes complete JSON by atomic replacement every 250 ms and on
exit. It reports `active`, `state`, `device`, connection count, total and session
frame/byte counters, sample count, latest normalized `peak`/`rms`, timestamps and
the last error. `received_frames`/`received_bytes` count validated incoming PCM;
`dropped_frames`/`dropped_bytes` count pending PCM discarded to keep audio current
or stop promptly. Levels become zero on disconnect or after one second without a
frame. Counters describe PCM submitted to the output, not independent proof that
a Windows application captured it. The status file never contains raw audio.

## Wire protocol

The Android client sends exactly 20 bytes: ASCII `ASMIC002`, then little-endian
uint32 values `48000`, `1`, `16`. The receiver validates the fixed format, opens
the selected output, and replies with ASCII `OKAY`. Every subsequent frame has
a little-endian uint32 byte length followed by signed 16-bit little-endian mono
PCM. Length must be positive, even and no larger than 1920 bytes (20 ms at
48 kHz). EOF between frames is a clean disconnect; EOF inside a frame is an
error. After each valid frame is enqueued, the receiver sends one byte `0x06`.
Android waits for that acknowledgment before sending another frame, bounding
audio held by TCP and the ADB bridge. Version 1 peers are rejected. The next
client can connect after either a clean disconnect or an error.

There is one active stream, a reusable receive buffer, five-second read deadlines,
one-second frame acknowledgment deadlines, a queue holding the two newest pending
frames (40 ms), and six native output buffers (120 ms). The independent socket
reader replaces the oldest pending frame when output is slower than capture.
It keeps consuming the stream so EOF can cancel a blocked native write immediately.
Output begins with the first block. A native buffer wait longer than 250 ms rejects
the stream. Windows audio-engine and VB-CABLE latency are additional. Disconnect
discards both pending PCM and native queued audio rather than draining them.
The six-buffer pool eliminated the paced-stream drops observed with three
buffers on the tested VB-CABLE setup: the paced regression submits all 500
frames in 10 seconds with no drops.

The optional native throughput tests send zero PCM to the specified cable without
recording or changing audio settings:

```powershell
$env:AUDIO_SHARE_MIC_TEST_DEVICE = 'CABLE Input'
go test -count=1 -v -run 'TestNativePCMThroughput|TestNativeReceiverKeepsUp' .
Remove-Item Env:AUDIO_SHARE_MIC_TEST_DEVICE
```

Both WAVEHDR and PCM live in VirtualAlloc memory while WinMM owns their pointers.
Cleanup calls waveOutReset, unprepares headers, releases native allocations, and
closes the output. If native cleanup fails, the process exits instead of keeping
possibly referenced memory alive across more sessions.

The implementation follows Microsoft's [audio data block lifecycle](https://learn.microsoft.com/en-us/windows/win32/multimedia/audio-data-blocks)
and [waveOutReset contract](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutreset).
