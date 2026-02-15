# CLAUDE.md — Mcaster1Studio AI Context

This file provides persistent context for Claude Code sessions working on Mcaster1Studio.
Update this file whenever architectural decisions change or new patterns are established.

---

## Project Identity

- **Name:** Mcaster1Studio
- **Type:** Broadcast Automation Software Suite (C++20 + Qt6)
- **Version:** 0.2.0-alpha (Phases 1–11 + A–I + CH + PD complete)
- **Root:** `C:/Users/dstjohn/dev/00_mcaster1.com/Mcaster1Studio/`
- **Part of:** Mcaster1 broadcast ecosystem

## Critical Paths

| Resource | Path |
|----------|------|
| vcpkg | `C:/Users/dstjohn/dev/vcpkg` |
| Qt 6.8.3 (USE THIS) | `C:/Qt/6.8.3/msvc2022_64` |
| Qt 6.9.1 msvc (incomplete) | `C:/Qt/6.9.1/msvc2022_64` — missing Qt6Config.cmake |
| Qt 6.10.2 msvc (incomplete) | `C:/Qt/6.10.2/msvc2022_64` — missing Qt6Config.cmake |

**Always use Qt 6.8.3 for builds until 6.9.1 or 6.10.2 MSVC is properly installed.**

## CMake Configure Command

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/dstjohn/dev/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
```

## Technology Stack

| Component | Choice |
|-----------|--------|
| Language | C++20 (CMAKE_CXX_STANDARD 20, EXTENSIONS OFF) |
| UI Framework | Qt 6.8.3 (Widgets + Multimedia + Network) |
| Build | CMake 3.28+ with VS2022 generator |
| Audio I/O | PortAudio 19.7+ with ASIO (vcpkg portaudio[asio]) |
| Plugin System | DLL C ABI (mcaster1_plugin_info, mcaster1_create_module) |
| Database | MySQL/MariaDB (libmysql via vcpkg) |
| Media Tags | TagLib 2.1.1 |
| HTTP | libcurl 8.18 |
| Audio Codecs | LAME (external), libopusenc, libvorbis, libFLAC, fdk-aac |
| Video | FFmpeg 8.0.1 |

## Surface Types

```cpp
enum class SurfaceType {
    Alpha, Beta, Company, DJ, Entertainment,
    Social, Podcast, Church, Custom
};
```

Surface Church = Live church services: PTT mic + Podcast recording + Live stream encoder +
VU Meter + Video capture (AV projection) + Media library (worship songs).

## Module ID Conventions

All built-in modules use reverse-DNS IDs: `com.mcaster1.<name>`
Effects use: `com.mcaster1.fx.<name>`

## ICY Protocol Rules

### CRITICAL — ALWAYS FOLLOW:
1. **All encoders MUST support ICY 1.x** for Icecast2, Shoutcast, and Mcaster1DNAS
2. **Mcaster1DNAS encoders MUST ALSO support ICY 2.2** (70+ fields, 8 groups)
3. **Always set StreamTitle= even when sending ICY 2.2** (backward compatibility)
4. **ICY 2.2 header negotiation:** Send `Icy-Version: 2.2` on SOURCE connection
5. **ICY 2.2 spec:** https://mcaster1.com/icy2-spec/

### ICY 2.2 Groups:
- Group 1: Station (icy2-station-*)
- Group 2: Show (icy2-show-*)
- Group 3: Track (icy2-track-*)
- Group 4: DJ (icy2-dj-*)
- Group 5: Social (icy2-social-*)
- Group 6: Podcast (icy2-podcast-*)
- Group 7: Broadcast (icy2-broadcast-*)
- Group 8: Content Flags (icy2-content-*)

### Encoder ICY compliance matrix:
- Icecast2 → ICY 1.x only
- Shoutcast v1/v2 → ICY 1.x only
- Mcaster1DNAS → ICY 1.x + ICY 2.2

## Theming System Rules

### NEVER do this in widget code:
- Call `setStyleSheet()` with hardcoded colors on individual widgets — this breaks theme switching and causes dark bleed
- Use font sizes below 12px in any widget stylesheet

### ALWAYS do this:
- `setObjectName("UniqueName")` on any widget needing custom theme colors
- Add QSS rules in all three theme files: `dark.qss`, `classic.qss`, `light.qss`
- Minimum font sizes: **12px** (body), **14px** (medium), **16px** (large/header) — Enterprise Pro requirement
- Transport/control SVG icons must be 32×32 viewBox with linearGradient + shine + feDropShadow for 3D look

### SVG icon pattern (3D raised style):
```svg
<defs>
  <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="#lightColor"/>
    <stop offset="55%" stop-color="#midColor"/>
    <stop offset="100%" stop-color="#darkColor"/>
  </linearGradient>
  <linearGradient id="shine" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0%" stop-color="rgba(255,255,255,0.50)"/>
    <stop offset="100%" stop-color="rgba(255,255,255,0)"/>
  </linearGradient>
  <filter id="sh"><feDropShadow dx="0" dy="2" stdDeviation="1.5" flood-opacity="0.30"/></filter>
</defs>
<rect x="1.5" y="1.5" width="29" height="29" rx="5.5" fill="url(#bg)" stroke="#darkestColor" stroke-width="1.2" filter="url(#sh)"/>
<rect x="2.5" y="2.5" width="27" height="14" rx="4.5" fill="url(#shine)"/>
<!-- icon in white -->
```

## Critical C++/Qt Patterns

### NEVER do this:
- `#include <version>` — conflicts with a file named VERSION on Windows. Solution: use `cmake/version.h.in`.
- Call TagLib `.toWString()` across DLL boundaries → use UTF-8 string bridge.
- Allocate memory or call Qt APIs inside `onAudioBlock()` / PortAudio callback.
- Use `QApplication::setStyle("Windows11")` — it ignores QSS. Use `"Fusion"`.
- Create a file literally named `VERSION` at project root (collides with C++20 `<version>`).

### ALWAYS do this:
- `qputenv("QT_MEDIA_BACKEND", "ffmpeg")` BEFORE `QApplication` — WMF backend can't decode OGG/Vorbis and has unreliable QAudioDecoder.
- `QApplication::setStyle(QStyleFactory::create("Fusion"))` for dark QSS theming.
- Compile any code using TagLib with `/EHa` (MSVC async exceptions) for corrupt file safety.
- Use `std::atomic<float>` for peak levels shared between RT audio thread and Qt timer thread.
- Include `MCASTER1PLUGIN_EXPORTS` define on plugin DLL targets.
- Use `qt_add_executable()` not `add_executable()` for Qt apps (for MOC, RCC, UIC).
- Use `Q_OBJECT` macro in every QObject-derived class that uses signals/slots.
- Add `Q_DECLARE_METATYPE()` for custom types used in queued connections.

### Audio Thread Safety:
- `onAudioBlock()` in IModule is called from PortAudio RT thread — NO Qt, NO alloc, NO mutex wait.
- Use `std::atomic<>` for data shared between RT thread and Qt thread.
- Use `QTimer` (Qt thread) to poll atomics and update UI (VUMeterWidget pattern).
- EventBus signals from AudioEngine are emitted via `QueuedConnection` to Qt thread.

### Plugin DLL Loading:
- Use `QLibrary` to load plugins.
- Call `mcaster1_plugin_info()` first to check `api_version == MCASTER1_PLUGIN_API_VERSION`.
- Plugins go in `plugins/modules/` (modules) and `plugins/effects/` (DSP units).

## Database (Phase H)

- **IDatabase** abstract interface (`Core/include/IDatabase.h`) — pure virtual base
- **SqliteManager** (default) — embedded SQLite3, zero-config, WAL mode, `AppData/Mcaster1Studio/mcaster1studio.db`
- **DatabaseManager** — MySQL/MariaDB, for enterprise multi-user deployments
- **Factory:** `MediaLibraryModule::createDatabase()` reads `QSettings("database/backend")` → "sqlite" (default) or "mysql"
- **Fallback:** If MySQL connection fails, auto-falls back to SQLite
- CMake targets: `unofficial::sqlite3::sqlite3` (SQLite), `unofficial::libmysql::libmysql` (MySQL)
- PreferencesDialog Database tab: backend selector, SQLite path, MySQL connection settings

## Phase Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Foundation: Shell + AudioEngine + VU Meters | ✅ Complete |
| 2 | Deck Module (dual player, waveform, crossfader) | ✅ Complete |
| 3 | Media Library Module (MySQL, TagLib, drag-to-deck) | ✅ Complete |
| 4 | Encoder Module — 7 codecs, 7-state, DSP chain, list-view, reconnect watchdog | ✅ Complete |
| 5 | Effects Rack Module + DSP plugins (Sonic, EQ31, Compressor) | ✅ Complete |
| 6 | Metadata Module (ICY 2.2 editor, 70+ fields, DNAS push) | ✅ Complete |
| 7 | Playlist / AutoDJ Module (clock template, broadcast logger) | ✅ Complete |
| 8 | Podcast Surface (PTTModule + PodcastModule, WAV export) | ✅ Complete |
| 9 | Video Module (QMediaPlayer, playlist, RTMP stub) | ✅ Complete |
| 10 | Monitor Module (Icecast2/Shoutcast/DNAS polling, chart) | ✅ Complete |
| F | Encoder Module Redesign — 7 codecs (MP3/Opus/Vorbis/FLAC/AAC), DSP chain, DNAS stats | ✅ Complete |
| G | UI Polish & Accessibility — Theme audit, DeckWidget 12px fonts, 3D SVG icons, tray nav | ✅ Complete |
| H | Audio Engine + SQLite DB — IDatabase, HTTP streaming, ICY 1.x/2.2, CartWall | ✅ Complete |
| I | Metadata Flow + VU Meters — Rich ICY from both decks, encoder VU panel, deck VU redesign | ✅ Complete |
| 11 | SDK + public plugin system — ModuleRegistry, IModuleHost, dynamic menus, SampleModule/Effect | ✅ Complete |
| CH | Church Surface — 12 modules (TimerClock, Graphics, Lyrics, Scripture, Announce, TelePrompt, MediaCaster, StageMon, AudioMix, TranscribeRec, SwitchCaster, ServiceRunner) | ✅ Complete |
| PD | Podcast Surface — 13 modules (PodMixer, PodPTT, PodRecorder, PodSoundboard, PodFX, PodEditor, PodEncode, PodTranscribe, PodShowNotes, PodRSS, PodPublisher, PodAnalytics, PodRemote) | ✅ Complete |
| 12 | Installer + polish | Pending |

## Module Wire-up (MainWindow)

Audio callback chain (RT thread): deck → PTT → cartwall → effects → podcast → encoder
- Playlist → Deck A: `requestLoadMedia` signal
- Deck A `finished()` → `PlaylistModule::advance()` when AutoDJ enabled
- Deck A `loadingFinished` → `MetadataModule::onMetadataUpdate()`
- Library → Deck: double-click/context-menu `requestLoadMedia`

### Church Surface Wiring (`wireChurchModules()`)
- GraphicsEngine → Lyrics, Scripture, Announce, MediaCaster (`setGraphicsEngine`)
- SwitchCaster → GraphicsEngine, Lyrics, Scripture, Announce, MediaCaster, Video (all sources)
- StageMon → GraphicsEngine, Lyrics, Scripture, TimerClock
- TelePrompt → GraphicsEngine
- ServiceRunner → TimerClock, SwitchCaster, TranscribeRec, AudioMix
- TranscribeRec → PTTModule (for mic input)
- All 12 modules discovered via `qobject_cast` scan of all surface modules

### Podcast Surface
- Per-instance factories, no singleton pointers in MainWindow
- No cross-module wiring needed (self-contained modules)

## Key Bug Fixes Applied

- `DeckPlayer::loadUrl()` — HTTP stream playback via StreamReader (FFmpeg + ICY 1.x/2.2)
- `DeckPlayer::loadFile()` auto-detects URLs and M3U/PLS playlists → routes to `loadUrl()`
- `StreamReader` (QThread) — FFmpeg `avformat_open_input()` with `icy=1` + `Icy-Version: 2.2` headers, SPSC ring buffer
- `DeckPlayer::processStreamBlock()` — reads from ring buffer, applies 3-band EQ, RT-safe
- `DeckPlayer::finished()` signal: 50ms EOF poll timer (`onEofPoll`)
- `ClockTemplate::slots` renamed to `clockSlots` (Qt `#define slots` macro conflict)
- `VideoWidget.h`: includes `VideoModule.h` (not forward-declare) for inner `State` enum
- PTT mic always-on bug: MouseButtonRelease sets State::Off (NOT State::Armed). Off→(press)→Live→(release)→Off.
- SurfaceTray focus: must call m_subTabBar->setCurrentIndex(idx) + m_stack->setCurrentIndex(idx) BEFORE panel->focusDock(dock)
- EncoderListWidget: use `QList<EncoderSlot*>* m_slots` (pointer, not reference) — MSVC C2832 cannot value-initialize reference members
- `slots` Qt macro: #define slots — never use as variable/parameter name in any context
- VU meters must measure post-gain levels (`sL * finalGain`) not pre-gain raw samples
- Both Deck A AND Deck B must wire `loadingFinished` → MetadataModule for ICY push
- `fabsf()` not `std::abs()` in RT code — MSVC can resolve `std::abs(float)` to int overload
- **Floating dock position restore (BF2):** NEVER clamp `move(x, y)` via `qBound(0, x, max(0, parentW - w))` when parent has zero dimensions (before `show()`) — collapses all to (0,0). Set raw position, clamp on first `QEvent::Resize`.
- **Deferred splitter sizes (BF2):** Use event filter `QEvent::Resize` on splitter, NOT `QTimer::singleShot`. Timers fail for hidden QStackedWidget panels where splitter width stays 0.

## Encoder Architecture (Phase F)

**7 Codecs:** MP3 (LAME, if found), Opus (libopusenc), Vorbis (libvorbis), FLAC (libFLAC), AAC-LC / HE-AAC-v1 / HE-AAC-v2 (fdk-aac, if found)

**7-State Machine:** Idle → Starting → Connecting → Streaming → Reconnecting → Sleep → Error
- `wake()` transitions Sleep → Connecting
- `setPttActive(bool)` forwards to EncoderDsp for duck attenuation

**Input Sources:** LiveDeckMix (ring buffer from onAudioBlock), PortAudio device, WASAPI loopback

**Per-slot DSP chain** (EncoderDsp, runs in encoder thread):
Input Gain → 10-band biquad EQ (RBJ, 6 presets) → Feedforward AGC + hard limiter → PTT duck (50ms ramp)

**RT thread** → `EncoderModule::onAudioBlock()` → `AudioRingBuffer::write()` (lock-free)
→ **Encoder thread** → `EncoderDsp::process()` → `encodePcm()` → `IcyPusher::push()` → TCP socket

**DnasPoller** — QThread background, polls `/admin/stats` XML every 15s, emits `statsUpdated` signal
**EncoderListWidget** — QTableWidget with 2s refresh timer, double-click → EncoderConfigDialog (5 tabs)

## Ecosystem Sibling Projects

| Project | Path | Notes |
|---------|------|-------|
| Mcaster1DNAS | `../mcaster1dnas/` | Streaming server, ICY 2.2, YAML config |
| Mcaster1DSPEncoder | `../Mcaster1DSPEncoder/` | Audio encoder reference for Phase 4 |
| Mcaster1TagStack | `../Mcaster1TagStack/` | MFC UI reference, ICY 2.2 editor reference |
| Mcaster1Castit | `../Mcaster1CastIt/` | Stats monitor reference for Phase 10 |

## PLANNING.html

The file `docs/PLANNING.html` is the living project plan and must be updated:
- After each phase completion
- When the surface list changes
- When the module catalog changes
- When dependencies change

## User Preferences (Confirmed)
- Audio engine: PortAudio + custom mixer (not JUCE)
- Plugin SDK: DLL C ABI factory functions (not QPluginLoader)
- Database: MySQL/MariaDB (not SQLite)
- Phase 1 target: Shell + Audio Engine + VU meters
- Qt style: Fusion (not Windows11) for dark QSS
- Build target: Windows 11 Pro x64, Debug first
