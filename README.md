# Audio Share
<p align="center">
    <img src="metadata/en-US/images/icon.png" width="20%" alt="metadata/en-US/images/icon.png">
</p>

[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/mkckr0/audio-share)](https://github.com/mkckr0/audio-share/releases/latest)
[![GitHub license](https://img.shields.io/github/license/mkckr0/audio-share)](https://img.shields.io/github/license/mkckr0/audio-share)
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fmkckr0%2Faudio-share.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fmkckr0%2Faudio-share?ref=badge_shield)
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
[![GitHub all releases](https://img.shields.io/github/downloads/mkckr0/audio-share/total)](https://img.shields.io/github/downloads/mkckr0/audio-share/total)
[![GitHub issues](https://img.shields.io/github/issues/mkckr0/audio-share)](https://img.shields.io/github/issues/mkckr0/audio-share)
[![GitHub closed issues](https://img.shields.io/github/issues-closed/mkckr0/audio-share)](https://img.shields.io/github/issues-closed/mkckr0/audio-share)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/mkckr0/audio-share)](https://img.shields.io/github/issues-pr/mkckr0/audio-share)
[![GitHub closed pull requests](https://img.shields.io/github/issues-pr-closed/mkckr0/audio-share)](https://img.shields.io/github/issues-pr-closed/mkckr0/audio-share)
[![Release](https://github.com/mkckr0/audio-share/actions/workflows/release.yml/badge.svg)](https://github.com/mkckr0/audio-share/actions/workflows/release.yml)
[![F-Droid](https://img.shields.io/f-droid/v/io.github.mkckr0.audio_share_app?logo=F-Droid)](https://f-droid.org/packages/io.github.mkckr0.audio_share_app)

<a href="https://f-droid.org/packages/io.github.mkckr0.audio_share_app"><img src="https://fdroid.gitlab.io/artwork/badge/get-it-on.png" height="75"></a>

Audio Share can share Windows/Linux computer's audio to Android phone over network, so your phone becomes the speaker of computer. (You needn't to buy a new speaker😄.)

## Usage for Windows GUI

- You need a computer with Windows 10 x86_64, and a phone with Android 6.0(API 23)+.
- Download app.apk and AudioShareServer.exe from [latest release](https://github.com/mkckr0/audio-share/releases/latest).
- Make sure AudioShareServer.exe in your computer and install audio-share-app-x.x.x-release.apk to your phone.
- Open AudioShareServer.exe in your computer and Audio Share app in your phone.
- Check all arguments are correct, especially the "Host" part. Make sure your phone can connect your computer over this IP.
- Click "Start Server" in AudioShareServer.exe and click "▶" button in app. Then enjoy the audio🎶.

> **Caution!!!**: This app doesn't support auto reconnecting feature at present. Once the app is killed  or disconnected by Android power saver, the audio playing will be stop. Adding app to the whitelist of power saver is recommended.

## Usage for Windows/Linux CMD




## Screenshot

<img src="docs/img/show_01.png" width="40%" alt="docs/img/show_01.png">

<img src="metadata/en-US/images/phoneScreenshots/1.png" width="30%" alt="metadata/en-US/images/phoneScreenshots/1.png">&nbsp;
<img src="metadata/en-US/images/phoneScreenshots/2.png" width="30%" alt="metadata/en-US/images/phoneScreenshots/2.png">

## Compile from source

- Server side
    - vcpkg is required for install dependencies. The dependencies are below:   
    `vcpkg install asio:x64-windows-static-md protobuf:x64-windows-static-md spdlog:x64-windows-static-md`
    - Visual Studio 2022 with "Desktop development with C++" workload and "C++ MFC for latest v143 build tools (x86 & x64)" option is required for compiling.

- App side
    - Android Studio will import all dependencies automatically.

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=mkckr0/audio-share&type=Date)](https://star-history.com/#mkckr0/audio-share&Date)

## License
This project is licensed under the [Apache-2.0 license](https://opensource.org/license/apache-2-0) .
```
   Copyright 2022-2023 mkckr0 <https://github.com/mkckr0>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```

[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fmkckr0%2Faudio-share.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fmkckr0%2Faudio-share?ref=badge_large)

## Used third-party libraries

- [Asio](https://github.com/chriskohlhoff/asio) licensed under the [BSL-1.0 license](http://www.boost.org/LICENSE_1_0.txt).
- [Protocol Buffers](https://github.com/protocolbuffers/protobuf) licensed under the [LICENSE](https://github.com/protocolbuffers/protobuf/blob/main/LICENSE).
- [spdlog](https://github.com/gabime/spdlog) licensed under the [MIT license](https://github.com/gabime/spdlog/blob/v1.x/LICENSE).
- [{fmt}](https://github.com/fmtlib/fmt) licensed under the [LICENSE](https://github.com/fmtlib/fmt/blob/master/LICENSE).
- [cxxopts](https://github.com/jarro2783/cxxopts) licensed under the [MIT license](https://github.com/jarro2783/cxxopts/blob/master/LICENSE)
- [Netty](https://github.com/netty/netty) licensed under the [Apache-2.0 license](http://www.apache.org/licenses/LICENSE-2.0).
- [Material Components for Android](https://github.com/material-components/material-components-android) licensed under the [Apache-2.0 license](http://www.apache.org/licenses/LICENSE-2.0).
- [Protobuf Plugin for Gradle](https://github.com/google/protobuf-gradle-plugin) licensed under the [LICENSE](https://github.com/google/protobuf-gradle-plugin/blob/master/LICENSE).
- [PipeWire](https://gitlab.freedesktop.org/pipewire/pipewire) licensed under the [LICENSE](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/LICENSE).