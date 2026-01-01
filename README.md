# Mcaster1Studio

Professional broadcast automation suite for radio, podcast, church, and live streaming.

## Overview

Mcaster1Studio is a modular broadcast control surface application built with C++20 and Qt 6.8.3. It provides purpose-built workspaces ("surfaces") for different broadcast scenarios, each pre-loaded with the modules needed for that workflow.

## Surfaces

| Surface | Purpose | Default Modules |
|---------|---------|----------------|
| Alpha | Primary radio broadcast | VU, Deck, Playlist, Encoder, Metadata |
| Beta | Secondary/backup broadcast | VU, Deck, Encoder |
| Company | Corporate/business streaming | Playlist, Encoder, Metadata, Monitor |
| DJ | Live DJ performance | VU, Deck, Effects, Library |
| Entertainment | TV/Video automation | Video, Encoder, Playlist |
| Social | Socialcasting + RTMP | Encoder, Metadata, Monitor |
| Podcast | Professional podcast production | 17 modules (mixer, recorder, editor, RSS, etc.) |
| Church | Live church services | 17 modules (lyrics, scripture, switcher, etc.) |
| Custom | User-defined | User chooses |

## Key Features

- **Dual-deck audio player** with waveform display, crossfader, and EQ
- **7-codec encoder** (MP3, Opus, Vorbis, FLAC, AAC-LC, HE-AAC v1/v2) with per-slot DSP chain
- **ICY 2.2 metadata** (70+ fields, 8 groups) for Mcaster1DNAS; ICY 1.x for Icecast2/Shoutcast
- **Media library** with TagLib scanning, search, and drag-to-deck
- **Playlist / AutoDJ** with clock-based scheduling and broadcast logger
- **Effects rack** with Sonic Enhancer, 31-band EQ, and multi-band compressor
- **Server monitor** polling Icecast2, Shoutcast, and Mcaster1DNAS stats
- **HTTP stream playback** via FFmpeg with ICY metadata receive
- **Theme system** (Dark, Classic, Light) with QSS stylesheets
- **Plugin SDK** with DLL C ABI for third-party modules and effects

## Build Requirements

- Windows 11 Pro x64
- Visual Studio 2022 (v143 toolset)
- CMake 3.28+
- Qt 6.8.3 MSVC 2022 64-bit
- vcpkg (portaudio, ffmpeg, taglib, sqlite3, lame, opus, vorbis, flac, fdk-aac, curl)

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/dstjohn/dev/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"

cmake --build build --config Debug
```

## Architecture

- **Core** — AudioEngine (PortAudio + ASIO), EventBus, IModule interface, IDatabase, ModuleRegistry
- **AudioEngine** — PortAudio RT callback, AudioMixer, DeckPlayer, StreamReader
- **Modules** — 40+ built-in modules as static libraries with plugin DLL C ABI
- **UI** — MainWindow, SurfaceWidget, SubSurfacePanel, ModuleDock, AppRibbon, ThemeManager

## Part of the Mcaster1 Ecosystem

- **Mcaster1DNAS** — Streaming server with ICY 2.2 support
- **Mcaster1DSPEncoder** — Standalone audio encoder with DSP plugin support
- **Mcaster1TagStack** — Metadata composer and media library manager
- **Mcaster1CastIt** — Server stats monitor and analytics

## License

Proprietary. Copyright (c) 2026 Mcaster1.
