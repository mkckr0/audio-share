# USB phone microphone for Windows (experimental)

Send the Android microphone to Windows applications over USB while continuing to use the phone as a speaker. The Android app has independent microphone and speaker controls. The Windows microphone receiver and speaker server coordinate their audio routes within the same Windows sign-in session. Update both Windows executables from this branch to enable this protection. Existing Android playback clients use the same wire protocol.

The microphone transport uses USB. The receiver is a separate Go/WinMM build target on Windows.

## Requirements

- Android 6 or newer, with USB debugging authorized for the computer. The local integration is tested on Pixel 4 / Android 13.
- Windows x64 and the signed [VB-CABLE driver](https://vb-audio.com/Cable/), installed separately from its official package.
- ADB and a USB cable. The microphone listener is restricted to localhost and uses only `adb reverse tcp:65529 tcp:65529`.

## Use

1. Install the `Audio Share + Mic` APK. This build is a side-by-side app, preserving an existing official Audio Share installation.
2. Build and start the Windows microphone receiver as described below. The optional PowerShell helper restores the microphone's USB forwarding after the phone reconnects.
3. In Audio Share + Mic, enable **Windows microphone** and grant microphone permission. Keep notifications enabled to use the notification's Stop action.
4. In your voice/meeting app, select **CABLE Output (VB-Audio Virtual Cable)** as the microphone. Send the app's playback to the regular PC playback device and select that same device in the Audio Share speaker server. The receiver writes the phone microphone to **CABLE Input**.
5. Turn off the phone's microphone switch to stop capture. The notification has another Stop control. Microphone capture does not start automatically at boot.

### Sharing a playback endpoint

When both services select the same playback endpoint, the speaker server pauses forwarding while the microphone is active. It resumes after the cable has drained. This also works if the microphone starts after a speaker client has connected.

To hear PC audio when the cable is the only playback device, start the speaker server with `--allow-system-mix` alongside your usual options. The Windows GUI offers **Default playback (allow all PC outputs when sharing the microphone cable)**. On a shared endpoint this mode sends audio from **all Windows playback devices** and excludes the Audio Share microphone receiver. Windows 10 build 20348 or newer is required for process exclusion. If Windows cannot start the filtered stream, the microphone continues working and the speaker server reports the error. [Windows application loopback capture](https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/)

With one cable, Windows apps recording **CABLE Output** also receive any PC app audio played directly into that cable. Choose a separate PC playback endpoint when a calling app needs a microphone-only input. Android's acoustic echo cancellation remains device- and room-dependent.

The cable can still contain microphone audio after Windows closes the render stream. The speaker server keeps the previous receiver excluded for one second, or pauses if the buffered samples could belong to two different receivers. It also waits one second when starting on a cable with unknown recent activity. Physical playback endpoints can start immediately. [VB-CABLE buffer sizes and internal sample rates](https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf#page=10)

## Windows receiver setup

For direct use, build the executable, run `adb -s YOUR_USB_SERIAL reverse tcp:65529 tcp:65529`, and start `audio-share-mic.exe`. No Windows firewall rule is required because the listener binds only to loopback.

For optional background operation, copy `audio-share-mic.exe` and `scripts/windows/Microphone.ps1` into one directory. Copy `microphone-settings.example.json` to `microphone-settings.json`, set `Serial` from `adb devices`, and set `AdbPath` to your ADB executable (absolute, or relative to the script). The helper supports one USB phone. It does not install startup tasks, change default audio devices or USB modes, restart the ADB server, or remove other reverse mappings.

```powershell
.\audio-share-mic.exe --list-devices
.\Microphone.ps1 -Action Start
.\Microphone.ps1 -Action Status
.\Microphone.ps1 -Action Stop
```

The default receiver output name is `CABLE Input`. Some Windows Sound panels label it `Speakers (VB-Audio Virtual Cable)` while WinMM still reports `CABLE Input`; use the names returned by `--list-devices`.

The helper's Stop action stops the computer receiver and disconnects this microphone transport; use the phone's switch to turn the phone feature off. Speaker playback is independent. The passive ADB port mapping is retained until USB disconnect, because ADB provides no ownership token that would make removing a recreated mapping safe. It carries no audio without the receiver. Logs and the status JSON contain connection/level information, not audio recordings.

## Build

Android uses the existing Gradle wrapper, JDK 17 and Android SDK 35. Run `gradlew.bat testMicrophoneUnitTest assembleMicrophone lintMicrophone` from `android-app`. The upstream release signing key is not required for the local build. Keep your local signing key if you need to install later updates over this build. Official APK update checks are disabled for the side-by-side package; install a new local build to update it.

The receiver requires Go 1.23 or newer, with no third-party Go dependencies, and uses Windows WinMM:

```powershell
cd server-mic
go test ./...
go vet ./...
go build -trimpath -o audio-share-mic.exe .
```

## Wire format

The client sends ASCII `ASMIC002`, followed by three little-endian uint32 values: sample rate 48000, channel count 1, sample bits 16. The receiver opens the requested device and replies ASCII `OKAY`.

Each subsequent frame is a little-endian uint32 byte count and that many PCM bytes. Samples are signed 16-bit little-endian. Length must be positive, even and at most 1920 bytes (20 ms). The receiver acknowledges every accepted frame with one byte `0x06`; the phone waits for this ACK before sending another frame. An independent receiver task drains transport into a two-frame queue, replacing the oldest pending frame on overflow. Oversized, malformed or truncated input ends the connection; a new client may reconnect. EOF or Stop cancels playback and discards queued output.

## License

Audio Share and this extension use Apache-2.0; upstream notices are retained. VB-CABLE is a separate proprietary donationware driver and is not included in this source distribution.
