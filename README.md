# AudioStream

AudioStream is a high-performance, low-latency, bidirectional audio streaming system between Windows PCs and Android devices over USB tethering and Wi-Fi.

## Features

- **PC → Android Streaming**: Low-latency desktop audio streaming with Opus compression or lossless uncompressed PCM.
- **Android → PC Virtual Microphone**: On-demand phone microphone streaming into Windows virtual audio input (VB-CABLE).
- **Zero-Setup Auto-Connection**: Automatic discovery and instant pairing over USB tethering (`192.168.42.x`) and UDP beacon broadcast.
- **Adaptive Jitter Buffer**: Dynamic packet loss concealment and configurable latency presets (Low, Medium, High).
- **Modern Desktop GUI**: Lightweight Dear ImGui interface with DirectX 11 backend, live audio meters, latency graphs, and system tray minimization.
- **Jetpack Compose Android App**: Modern Material 3 dark interface with per-channel volume, bitrate controls, and background audio service.

## Architecture

```
                    ┌─────────────────────────────────────────────────────────────┐
                    │                      WINDOWS DESKTOP PC                     │
                    │                                                             │
                    │  ┌────────────────────┐         ┌──────────────────────┐    │
                    │  │  WASAPI Loopback   │         │     Opus Encoder     │    │
                    │  │ (Desktop Audio Out)├────────►│  (48kHz Stereo VBR)  │    │
                    │  └────────────────────┘         └──────────┬───────────┘    │
                    │                                            │                │
                    │                                            ▼                │
                    │  ┌────────────────────┐         ┌──────────────────────┐    │
                    │  │  VirtualMicMonitor │         │    NetworkManager    │    │
                    │  │  (IAudioSessionMgr2│         │ (TCP:65530, UDP:65530│    │
                    │  └─────────┬──────────┘         └──────────▲───────────┘    │
                    │            │                               │                │
                    │  ┌─────────▼──────────┐         ┌──────────┴───────────┐    │
                    │  │   VB-CABLE Input   │         │  MicReceiver & Jitter│    │
                    │  │  (WASAPI Render)   │◄────────┤    Opus Decoder      │    │
                    │  └────────────────────┘         └──────────────────────┘    │
                    └────────────────────────────────────────────▲────────────────┘
                                                                 │
                            USB Tethering (192.168.42.x) / Wi-Fi │ UDP: 0x01 PC Audio
                            TCP Control Commands (1..9)          │ UDP: 0x02 Mic Audio
                                                                 │
                    ┌────────────────────────────────────────────▼────────────────┐
                    │                        ANDROID PHONE                        │
                    │                                                             │
                    │  ┌────────────────────┐         ┌──────────────────────┐    │
                    │  │     NetClient      │────────►│ PlaybackService &    │    │
                    │  │ (TCP/UDP Dispatch) │         │ Opus Decoder / Track ├─► Speakers
                    │  └─────────▲──────────┘         └──────────────────────┘    │
                    │            │                                                │
                    │  ┌─────────┴──────────┐         ┌──────────────────────┐    │
                    │  │ MicCaptureService  │◄────────┤ Phone Microphone     │    │
                    │  │ (JNI Opus Encoder) │         │ (AudioRecord Source) │    │
                    │  └────────────────────┘         └──────────────────────┘    │
                    └─────────────────────────────────────────────────────────────┘
```

## Protocol Specifications

1. **Control Transport**: TCP port 65530 (Commands: `CMD_GET_FORMAT` 0x01, `CMD_START_PLAY` 0x02, `CMD_HEARTBEAT` 0x03, `CMD_NEGOTIATE_CODEC` 0x04, `CMD_START_MIC` 0x05, `CMD_STOP_MIC` 0x06).
2. **Discovery Broadcast**: UDP port 59200 broadcasting `DiscoveryBeacon` protobuf every 2.0s.
3. **Audio Framing**: UDP port 65530 with 7-byte binary header:
   - Byte 0: Packet Type (`0x01` = Desktop PC Audio, `0x02` = Phone Mic Audio).
   - Bytes 1–2: `uint16_t` Little-Endian Sequence Number.
   - Bytes 3–6: `uint32_t` Little-Endian RTP Timestamp.
   - Bytes 7+: Audio Payload (Opus frame or raw PCM samples).

## Building from Source

### Windows Desktop Application (`AudioStream.exe`)
- Prerequisites: CMake 3.20+, MinGW-w64 or MSVC C++20 compiler.
- Dependencies: `libopus`, `asio`, `protobuf`, `imgui`, `spdlog`.
- Build output placed in `dist/AudioStream.exe`.

### Android Client (`AudioStream.apk`)
- Prerequisites: JDK 17, Android SDK API 35.
- Build command: `./gradlew :app:assembleDebug` or `:app:assembleRelease`.
- Build output placed in `dist/AudioStream.apk`.

## License
AudioStream is licensed under the Apache License, Version 2.0.
