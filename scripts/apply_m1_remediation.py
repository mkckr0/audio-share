#!/usr/bin/env python3
"""
AudioStream Milestone 1 Master Remediation Script
Applies all file deletions, directory migrations, and brand substitutions in-place.
"""

import os
import shutil
import re

REPO_ROOT = "/workspaces/audio-share"

def remove_path(rel_path):
    full_path = os.path.join(REPO_ROOT, rel_path)
    if os.path.isdir(full_path):
        shutil.rmtree(full_path)
        print(f"[DELETED DIR]  {rel_path}")
    elif os.path.isfile(full_path):
        os.remove(full_path)
        print(f"[DELETED FILE] {rel_path}")

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)

def write_file(rel_path, content):
    full_path = os.path.join(REPO_ROOT, rel_path)
    ensure_dir(os.path.dirname(full_path))
    with open(full_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"[WRITTEN]      {rel_path}")

def replace_in_file(rel_path, replacements):
    full_path = os.path.join(REPO_ROOT, rel_path)
    if not os.path.isfile(full_path):
        print(f"[SKIP MISSING] {rel_path}")
        return
    with open(full_path, "r", encoding="utf-8") as f:
        content = f.read()
    for old_str, new_str in replacements:
        content = content.replace(old_str, new_str)
    with open(full_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"[MODIFIED]     {rel_path}")

def regex_replace_in_file(rel_path, patterns):
    full_path = os.path.join(REPO_ROOT, rel_path)
    if not os.path.isfile(full_path):
        print(f"[SKIP MISSING] {rel_path}")
        return
    with open(full_path, "r", encoding="utf-8") as f:
        content = f.read()
    for pattern, repl in patterns:
        content = re.sub(pattern, repl, content)
    with open(full_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"[REGEX MOD]    {rel_path}")

def main():
    print("=== 1. Obsolete Asset & File Deletions ===")
    remove_path("server-mfc")
    remove_path("metadata")
    remove_path("docs/img/win_01.png")
    remove_path("docs/img/win_02.png")
    if os.path.isdir(os.path.join(REPO_ROOT, "docs/img")) and not os.listdir(os.path.join(REPO_ROOT, "docs/img")):
        os.rmdir(os.path.join(REPO_ROOT, "docs/img"))
        print("[DELETED EMPTY DIR] docs/img")
    remove_path("scripts/setup_sourceforge_release.sh")
    remove_path(".github/workflows/update_sourceforge.yml")
    remove_path("protos/client.proto")
    remove_path("android-app/app/src/main/res/drawable/github_mark.xml")
    remove_path("android-app/app/src/main/res/drawable/github_mark_white.xml")
    remove_path("android-app/app/src/main/res/drawable-v24/github_mark.xml")

    print("\n=== 2. Write Extended Protobuf Schema ===")
    AUDIOSTREAM_PROTO = """syntax = "proto3";

package com.audiostream.pb;

option java_package = "com.audiostream.app.pb";
option java_outer_classname = "AudioStreamProto";

enum AudioCodec {
  CODEC_PCM = 0;
  CODEC_OPUS = 1;
}

enum AudioEncoding {
  ENCODING_INVALID = 0;
  ENCODING_PCM_FLOAT = 1;
  ENCODING_PCM_8BIT = 2;
  ENCODING_PCM_16BIT = 3;
  ENCODING_PCM_24BIT_PACKED = 4;
  ENCODING_PCM_32BIT = 5;
}

message AudioFormat {
  AudioEncoding encoding = 1;
  int32 channels = 2;
  int32 sample_rate = 3;
}

message StreamConfig {
  AudioFormat format = 1;
  AudioCodec codec = 2;
  int32 opus_bitrate = 3;
  int32 opus_frame_size_ms = 4;
  int32 target_latency_ms = 5;
}

message MicControl {
  bool start = 1;
  StreamConfig config = 2;
  int32 mic_source = 3;
}

message DiscoveryBeacon {
  string app_name = 1;
  string server_name = 2;
  int32 port = 3;
  string version = 4;
  bool supports_opus = 5;
  bool supports_mic = 6;
  string ip_address = 7;
}
"""
    write_file("protos/audiostream.proto", AUDIOSTREAM_PROTO)

    print("\n=== 3. Root Level Files Debranding ===")
    write_file("VERSION", "1.0.0\n")

    LICENSE_CONTENT = """                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   Copyright 2026 AudioStream Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
"""
    write_file("LICENSE", LICENSE_CONTENT)

    README_CONTENT = """# AudioStream

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
"""
    write_file("README.md", README_CONTENT)

    # Scripts debranding
    APPLY_VERSION_SH = """#!/bin/bash
pushd $(dirname $0)/.. &>/dev/null

version_name=$(bash ./scripts/get_version.sh -n)
version_code=$(bash ./scripts/get_version.sh -c)

echo VERSION: \\
    version_name=$version_name \\
    version_code=$version_code

if [ -f "android-app/app/build.gradle.kts" ]; then
    sed -Ebi "s|versionName\\s*=\\s*\\\"[^\\\"]*\\\"|versionName = \\\"$version_name\\\"|g" android-app/app/build.gradle.kts
    sed -Ebi "s|versionCode\\s*=\\s*[0-9]*|versionCode = $version_code|g" android-app/app/build.gradle.kts
fi

if [ -f "server/CMakeLists.txt" ]; then
    sed -Ebi "s|\\tVERSION\\s+[0-9.]*|\\tVERSION $version_name|g" server/CMakeLists.txt
fi

popd &>/dev/null
"""
    write_file("scripts/apply_version.sh", APPLY_VERSION_SH)

    # Release workflow debranding
    regex_replace_in_file(".github/workflows/release.yml", [
        (r'build_server_mfc:\n.*?android-app/app/build/outputs/apk/release/\*\.sha256\n', ''),
        (r'needs:\s*\[build_app,\s*build_server_mfc,\s*build_server_core\]', 'needs: [build_app, build_server_core]'),
        (r'echo -e "\$\(cat \./metadata/.*?\)" > notes', 'echo "AudioStream Release v$version" > notes'),
        (r'mkckr0/audio-share', 'audiostream/audiostream'),
        (r'io\.github\.mkckr0\.audio_share_app', 'com.audiostream.app')
    ])

    print("\n=== 4. Android Package Migration & Debranding ===")
    
    # 4.1 Gradle and Resource Configs
    replace_in_file("android-app/settings.gradle.kts", [
        ('rootProject.name = "audio-share-app"', 'rootProject.name = "AudioStream"')
    ])

    regex_replace_in_file("android-app/app/build.gradle.kts", [
        (r'/\*\s*\*\s*Copyright 2022-2024 mkckr0.*?\*/\s*', ''),
        (r'namespace = "io\.github\.mkckr0\.audio_share_app"', 'namespace = "com.audiostream.app"'),
        (r'applicationId = "io\.github\.mkckr0\.audio_share_app"', 'applicationId = "com.audiostream.app"'),
        (r'versionCode = 3004', 'versionCode = 1000'),
        (r'versionName = "0\.3\.4"', 'versionName = "1.0.0"'),
        (r'base\.archivesName = ".*?"', 'base.archivesName = "AudioStream-1.0.0"')
    ])

    replace_in_file("android-app/app/proguard-rules.pro", [
        ("io.github.mkckr0.audio_share_app.pb", "com.audiostream.app.pb"),
        ("io.github.mkckr0.audio_share_app", "com.audiostream.app")
    ])

    replace_in_file("android-app/app/src/main/res/values/values.xml", [
        ('<string name="app_name" translatable="false">Audio Share</string>', '<string name="app_name" translatable="false">AudioStream</string>'),
        ('https://github.com/mkckr0/audio-share', 'https://github.com/audiostream/audiostream'),
        ('https://api.github.com/repos/mkckr0/audio-share', 'https://api.github.com/repos/audiostream/audiostream')
    ])

    if os.path.exists(os.path.join(REPO_ROOT, "android-app/.idea")):
        shutil.rmtree(os.path.join(REPO_ROOT, "android-app/.idea"))
        print("[DELETED] android-app/.idea")

    # 4.2 Move Kotlin source trees from io/github/mkckr0/audio_share_app to com/audiostream/app
    SRC_SETS = [
        ("android-app/app/src/main/java", "io/github/mkckr0/audio_share_app", "com/audiostream/app"),
        ("android-app/app/src/androidTest/java", "io/github/mkckr0/audio_share_app", "com/audiostream/app"),
        ("android-app/app/src/test/java", "io/github/mkckr0/audio_share_app", "com/audiostream/app")
    ]

    for base_dir, old_pkg_path, new_pkg_path in SRC_SETS:
        full_old_dir = os.path.join(REPO_ROOT, base_dir, old_pkg_path)
        full_new_dir = os.path.join(REPO_ROOT, base_dir, new_pkg_path)
        if os.path.isdir(full_old_dir):
            ensure_dir(os.path.dirname(full_new_dir))
            if os.path.exists(full_new_dir):
                shutil.rmtree(full_new_dir)
            shutil.move(full_old_dir, full_new_dir)
            print(f"[MOVED DIR]    {full_old_dir} -> {full_new_dir}")
            
            # Clean up empty parent directories
            io_dir = os.path.join(REPO_ROOT, base_dir, "io")
            if os.path.isdir(io_dir):
                shutil.rmtree(io_dir)
                print(f"[REMOVED OLD]  {io_dir}")

    # 4.3 Scrub all Kotlin and resource files
    for root, _, files in os.walk(os.path.join(REPO_ROOT, "android-app/app/src")):
        for f in files:
            fpath = os.path.relpath(os.path.join(root, f), REPO_ROOT)
            if f.endswith((".kt", ".xml", ".kts", ".pro", ".properties")):
                regex_replace_in_file(fpath, [
                    (r'/\*\s*\*\s*Copyright 2022-2024 mkckr0.*?\*/\s*', ''),
                ])
                replace_in_file(fpath, [
                    ("io.github.mkckr0.audio_share_app", "com.audiostream.app"),
                    ("io.github.mkckr0", "com.audiostream"),
                    ("Audio Share", "AudioStream"),
                    ("https://github.com/mkckr0/audio-share", "https://github.com/audiostream/audiostream"),
                    ("https://api.github.com/repos/mkckr0/audio-share/releases/latest", "https://api.github.com/repos/audiostream/audiostream/releases/latest"),
                    ("audio-share-app-", "AudioStream-"),
                    ("audio-share-app", "AudioStream"),
                    ("AudioShareServer", "AudioStream")
                ])
                # Specific fix in SettingsScreen.kt for deleted github_mark drawable
                if f == "SettingsScreen.kt":
                    replace_in_file(fpath, [
                        ("icon = R.drawable.github_mark,", "icon = Icons.Default.Info,"),
                    ])

    print("\n=== Milestone 1 Remediation Script Execution Complete ===")

if __name__ == "__main__":
    main()
