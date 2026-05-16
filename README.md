# Mcaster1Studio

**v0.4.0-beta** — Professional broadcast automation suite for radio, podcast, church, and live streaming.

## Overview

Mcaster1Studio is a modular broadcast control surface application built with C++20 and Qt 6.8.3. It provides purpose-built workspaces ("surfaces") for different broadcast scenarios, each pre-loaded with the modules needed for that workflow.

**Portable application** — all config, databases, logs, and cache stored inside the install directory. No AppData, no Registry, no roaming profiles.

## Surfaces

| Surface | Purpose | Default Modules |
|---------|---------|----------------|
| Alpha | Primary radio broadcast | VU, Deck, Playlist, Encoder, Metadata |
| Beta | Secondary/backup broadcast | VU, Deck, Playlist, Encoder |
| Company | Corporate/business streaming | Deck, Playlist, Encoder |
| DJ | Live DJ performance | VU, Deck, Playlist, Effects, Library, Encoder |
| Entertainment | TV/Video automation | Video, Encoder, Playlist |
| Social | Socialcasting + RTMP | Encoder, Metadata, Monitor |
| Podcast | Professional podcast production | 17 modules (mixer, recorder, editor, RSS, etc.) |
| Church | Live church services | 17 modules (lyrics, scripture, switcher, etc.) |
| Custom | User-defined | User chooses |

## Key Features

### Audio Engine
- **Dual-deck player** with crossfader, 3-band EQ, hot cues, loop, cue points, BPM detection
- **Playlist/AutoDJ** permanently embedded in DeckPlayer — 4 strategies (Random, Weighted, Category Rotation, Clock Wheel)
- **AuxDeck modules** with independent dual-bus audio routing (AIR + CUE on separate devices)
- **7-codec encoder** (MP3, Opus, Vorbis, FLAC, AAC-LC, HE-AAC v1/v2) with per-slot DSP chain
- **HTTP stream playback** via FFmpeg with ICY 1.x/2.2 metadata receive
- **Effects rack** with Sonic Enhancer, 31-band EQ, and multi-band compressor

### Media Library (ported from Mcaster1AMP)
- **FTS5 full-text search** with category scoping (LIKE fallback if FTS5 unavailable)
- **Library categories** — 7 presets (Music, Stingers, Station IDs, Sweepers, Jingles, Ads, Spoken Word) + custom
- **Album art cache** — 3-tier: memory LRU → disk SHA256 → TagLib extraction
- **AI Intel badges** — ColIntel column shows which artists have saved AI reports
- **Right-click actions** — Set Rating, AutoDJ Weight, Assign to Category, Play in AUX Deck on CUE device

### AI Integration (35% complete — testing/QA)
- **6 AI providers** — Ollama (local), Claude, ChatGPT, Grok, Gemini, Venice
- **15 preset AI personas** — Radio DJ (7 genres), Podcast Host (3 styles), Sports, TV News, Church, Social, Producer
- **Artist Intel Dialog** — 8 research buttons + Discovery, multi-turn chat, per-tab save/load
- **AI DJ Agent Browser** — conversational track recommendations with persona
- **Playlist Generator Pro** — AI-curated playlists with broadcast element insertion
- **Daypart Scheduler Pro** — 24h visual timeline with AI auto-schedule

### Broadcast
- **ICY 2.2 metadata** (70+ fields, 8 groups) for Mcaster1DNAS; ICY 1.x for Icecast2/Shoutcast
- **Server monitor** polling Icecast2, Shoutcast, and Mcaster1DNAS stats
- **Plugin SDK** with DLL C ABI for third-party modules and effects
- **5 database backends** — SQLite (default), MySQL/MariaDB, PostgreSQL, Firebird, SQL Server
- **Code signed** — SHA256 RSA 4096-bit certificate, SmartScreen trusted

### Theme System
- **Enterprise Pro** (default) — clean white/blue professional theme
- **Classic** — warm mahogany SAM Broadcaster 4 style
- **ThemePalette** central color registry — all widgets adapt to theme

## AuxDeck — Independent Audio Routing

Each AuxDeck module has its own dedicated PortAudio output streams:

| Bus | Purpose | Device | Toggle |
|-----|---------|--------|--------|
| AIR | On-air broadcast output | Independently selectable | Green when active |
| CUE | Headphone preview | Independently selectable | Blue when active |

**Use cases:** DJ cueing, interview playback, jingle deck, multi-room audio, recording preview.

Both buses can be active simultaneously on different physical devices. Same-device collision is prevented.

## Build Requirements

- Windows 11 Pro x64
- Visual Studio 2022 (v143 toolset)
- CMake 3.28+
- Qt 6.8.3 MSVC 2022 64-bit
- vcpkg (portaudio, ffmpeg, taglib, sqlite3, lame, opus, vorbis, flac, fdk-aac, curl)

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=<VCPKG_ROOT>/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_PREFIX_PATH="<QT_ROOT>/6.8.3/msvc2022_64"

cmake --build build --config Release
```

## Architecture

- **Core** — AudioEngine (PortAudio + ASIO), EventBus, IModule, IDatabase, ModuleRegistry, ThemePalette
- **AudioEngine** — PortAudio RT callback, AudioMixer, DeckPlayer, StreamReader
- **Modules** — 43+ built-in modules as static libraries with plugin DLL C ABI
- **UI** — MainWindow, SurfaceWidget, SubSurfacePanel, ModuleDock, AppRibbon, ThemeManager
- **AI** — AiTrackIntel (6 providers), PersonaManager (15 presets), ArtistIntelDialog, AlbumArtCache

## Part of the Mcaster1 Ecosystem

- **Mcaster1DNAS** — Streaming server with ICY 2.2 support
- **Mcaster1DSPEncoder** — Standalone audio encoder with DSP plugin support
- **Mcaster1AMP** — Professional broadcast media player (media library reference)
- **Mcaster1AudioPipe** — Virtual audio routing between applications
- **Mcaster1TagStack** — Metadata composer and media library manager
- **Mcaster1CastIt** — Server stats monitor and analytics

## License

Proprietary. Copyright (c) 2026 Mcaster1 Software.
