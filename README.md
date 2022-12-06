# Audio Share

[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/mkckr0/audio-share)](https://github.com/mkckr0/audio-share/releases/latest)
[![GitHub](https://img.shields.io/github/license/mkckr0/audio-share)](https://img.shields.io/github/license/mkckr0/audio-share)
[![GitHub Release Date](https://img.shields.io/github/release-date/mkckr0/audio-share)](https://img.shields.io/github/release-date/mkckr0/audio-share)
[![GitHub last commit](https://img.shields.io/github/last-commit/mkckr0/audio-share)](https://img.shields.io/github/last-commit/mkckr0/audio-share)
[![GitHub contributors](https://img.shields.io/github/contributors/mkckr0/audio-share)](https://img.shields.io/github/contributors/mkckr0/audio-share)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/y/mkckr0/audio-share)](https://img.shields.io/github/commit-activity/y/mkckr0/audio-share)
[![GitHub Repo stars](https://img.shields.io/github/stars/mkckr0/audio-share)](https://img.shields.io/github/stars/mkckr0/audio-share)
[![GitHub forks](https://img.shields.io/github/forks/mkckr0/audio-share)](https://img.shields.io/github/forks/mkckr0/audio-share)
[![GitHub watchers](https://img.shields.io/github/watchers/mkckr0/audio-share)](https://img.shields.io/github/watchers/mkckr0/audio-share)
[![GitHub language count](https://img.shields.io/github/languages/count/mkckr0/audio-share)](https://img.shields.io/github/languages/count/mkckr0/audio-share)
[![GitHub top language](https://img.shields.io/github/languages/top/mkckr0/audio-share)](https://img.shields.io/github/languages/top/mkckr0/audio-share)
[![GitHub repo size](https://img.shields.io/github/repo-size/mkckr0/audio-share)](https://img.shields.io/github/repo-size/mkckr0/audio-share)
[![Lines of code](https://img.shields.io/tokei/lines/github/mkckr0/audio-share)](https://img.shields.io/tokei/lines/github/mkckr0/audio-share)
[![GitHub all releases](https://img.shields.io/github/downloads/mkckr0/audio-share/total)](https://img.shields.io/github/downloads/mkckr0/audio-share/total)
[![GitHub issues](https://img.shields.io/github/issues/mkckr0/audio-share)](https://img.shields.io/github/issues/mkckr0/audio-share)
[![GitHub closed issues](https://img.shields.io/github/issues-closed/mkckr0/audio-share)](https://img.shields.io/github/issues-closed/mkckr0/audio-share)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/mkckr0/audio-share)](https://img.shields.io/github/issues-pr/mkckr0/audio-share)
[![GitHub closed pull requests](https://img.shields.io/github/issues-pr-closed/mkckr0/audio-share)](https://img.shields.io/github/issues-pr-closed/mkckr0/audio-share)
[![Build App](https://github.com/mkckr0/audio-share/actions/workflows/build_app.yml/badge.svg)](https://github.com/mkckr0/audio-share/actions/workflows/build_app.yml)
[![Build Server](https://github.com/mkckr0/audio-share/actions/workflows/build_server.yml/badge.svg)](https://github.com/mkckr0/audio-share/actions/workflows/build_server.yml)

Audio Share can share Windows computer's audio to Android phone over network, so your phone becomes the speaker of computer. (You needn't to buy a new speaker😄.)

## Usage

- You need a computer with Windows 10 x86_64, and a phone with Android 9.0(Pie) API 28.
- Download app-debug.apk and AudioShareServer.exe from [latest release](https://github.com/mkckr0/audio-share/releases/latest).
- Make sure AudioShareServer.exe in your computer and install app-debug.apk to your phone.
- Open AudioShareServer.exe in your computer and Audio Share app in your phone.
- Check all arguments are correct, especially the "Host" part. Make sure your phone can connect your computer over this IP.
- Click "Start Server" in AudioShareServer.exe and click "START" in app. Then enjoy the audio🎶.
> **Caution!!!**: This app doesn't support auto reconnecting feature at present. Once the app is killed  or disconnected by Android power saver, the audio playing will be stop. Adding app to the whitelist of power saver is recommended.

## Screenshot

<img src="docs/img/show_01.jpg" width="40%" alt="show_01.jpg">
<img src="docs/img/show_02.jpg" width="20%" alt="show_02.jpg">

## Compile from source

- Server side
    - vcpkg is required for install dependencies. The dependencies are below:   
    `vcpkg install asio:x64-windows-static-md protobuf:x64-windows-static-md spdlog:x64-windows-static-md`
    - Visual Studio 2022 with "Desktop development with C++" workload and "C++ MFC for latest v143 build tools (x86 & x64)" option is required for compiling.

- App side
    - Android Studio will import all dependencies automatically.
