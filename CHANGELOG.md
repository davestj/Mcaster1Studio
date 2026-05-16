# Changelog — Mcaster1Studio

All notable changes to Mcaster1Studio are documented in this file.

## [0.4.0-beta] — 2026-03-19

### Theme System Overhaul
- Deleted dark.qss — Enterprise Pro is now the default theme
- ThemePalette central color registry (Core/include/ThemePalette.h)
- Stripped 200+ hardcoded setStyleSheet calls across 45+ files
- Module dock framing with steel-blue borders on white cards
- Font size compliance — all violations raised to 12px minimum

### Media Library Upgrade (ported from Mcaster1AMP)
- FTS5 full-text search (LIKE fallback if FTS5 unavailable)
- Library categories sidebar — 7 preset (Music, Stingers, Station IDs, Sweepers, Jingles, Ads, Spoken Word)
- Hierarchical category tree (3+ levels, expandable/collapsible)
- Album art cache (memory + disk + TagLib extraction)
- AI Intel badges in track list (ColIntel column)
- Right-click: Set Rating (0-5 stars), AutoDJ Weight, Assign to Category
- Right-click: Play in AUX Deck on CUE device
- Scan-into-category with auto-assignment
- FTS5 search scoped to active category
- 7 new database tables + schema migration for existing databases

### AI Persona System
- 15 preset AI personas (7 Radio DJ genres, 3 Podcast styles, Sports, TV News, Church, Social, Producer)
- ai_personas + daypart_schedule tables in all 5 SQL dialects
- PersonaManager with 3-tier resolution (daypart > surface > global)
- Preferences > AI: persona combo + editable system prompt (500 char max)
- Per-category persona assignment via right-click context menu
- Personas power AI recommendations, playlist generation, and artist intel

### Artist Intel Dialog
- Full window: Overview, Discography, Images, DJ Script tabs
- 8 AI Research buttons (Touring History, Musical Influences, Awards & Charts, Broadcaster Script, Fan Base & Impact, Full Timeline, Gear & Equipment, Discovery)
- Multi-turn "Ask AI" chat with conversation context
- Save/load per-tab as JSON (overview + research tabs independently)
- Right-click tab: Refresh Report, Save Individual Report as HTML
- Auto-retry on busy (5s delay), user-friendly error messages
- Save confirmation dialog showing DB path, table, record ID
- AI Intel badges appear in library track list after saving

### AI-Powered Category Features
- AI DJ Agent Browser — interactive conversational recommendations with persona
- Playlist Generator Pro — source selector, AI Intel prioritization, broadcast element insertion
- Daypart Scheduler Pro — 24h visual timeline, persona per time block, AI auto-schedule
- Animated braille spinner progress indicators with phase messages

### DeckPlayer Module
- Crossfader top-aligned between decks, PTT below
- Playlist/AutoDJ permanently embedded in center column (DO NOT REMOVE)
- AutoDJ auto-plays first track on enable
- DeckA/DeckB rich ICY metadata (artist, title, album, genre, year, BPM)
- S-curve crossfade gain fix, loop boundary guard, ring buffer overflow fix
- libmp3lame.dll auto-copy via CMake post-build

### AuxDeck Module v3.0
- Full DeckPlayer-class: seek, hot cues (4), loop, cue point, pitch/speed
- BPM display with ±2% nudge, album art, metadata stats grid, state badge
- Tabbed panel: Track History, Recordings, Config
- Recording capability (WAV format, auto-naming)
- Per-deck PortAudio output stream for independent CUE device routing
- CUE/AIR device combos apply immediately on selection
- Dual-bus audio routing — AIR and CUE each have independent PortAudio output streams
- AIR/CUE toggle buttons flanking Mute — fully independent on/off control
- Both buses can output simultaneously to different physical devices
- Device collision prevention — same device blocked on both AIR and CUE
- Per-bus routing: dedicated PA stream when specific device selected, global fallback when "(Use Global Default)"
- 26 output devices enumerated via PortAudio (WASAPI, DirectSound, MME, ASIO)

### Infrastructure
- Sub-surface tab switching fix (panel created before currentChanged fires)
- Portable app: all QSettings → INI in <appDir>/config/
- Code signing: CMake post-build + NSIS cert import + installer signing
- Version bump cadence: VERSION.txt, CHANGELOG, docs, installer all in sync
- SQL dialect sync: every new table added to all 5 backends simultaneously
- Preferences dialog: minimize/maximize/resize buttons, screen-aware sizing, centered on available geometry
- AI Integration page wrapped in QScrollArea for long content
- STANDARDS.md created — mandatory rules for portable storage, SQL dialects, theme system, version bumps
- Waveform placeholder changed from "DROP AUDIO FILE" to "NO TRACK LOADED"

## [0.3.1-beta] — 2026-03-17

### Portable Self-Contained Architecture
- **All data stays in install directory** — config, databases, logs, themes, plugins
- QSettings redirected from Windows Registry to `<appDir>/config/Mcaster1/Mcaster1Studio.ini` (INI format)
- SQLite databases default to `<appDir>/data/` instead of AppData
- Surface YAML configs moved to `<appDir>/config/surfaces/`
- Debug logs and crash dumps moved to `<appDir>/logs/`
- Zero writes to AppData, LocalAppData, Registry, or Roaming profiles

### Code Signing
- **CMake post-build** auto-signs Mcaster1Studio.exe via signtool (SHA256, RSA 4096-bit cert)
- **NSIS installer** imports .cer to current-user Root + TrustedPublisher stores before binaries land
- **Installer .exe** itself is signed — eliminates SmartScreen warnings
- Shared cert covers all Mcaster1 apps (Studio, AudioPipe, AMP, DSPEncoder)

### Installer Overhaul
- NSIS installer output: `Mcaster1Studio-0.3.1-beta-setup.exe`
- Installs to `C:\Users\<username>\Mcaster1\Mcaster1Studio` (no admin, user profile)
- Wildcard DLL staging — copies ALL build output DLLs, nothing missed
- MSVC C++ runtime DLLs (msvcp140, vcruntime140, concrt140) bundled from VS2022 Redist
- libmp3lame.dll auto-discovered from known locations
- Fresh zeroed-out config INI created on new install (preserved on upgrade)
- Portable directory structure: config/, data/, logs/, themes/, docs/, plugins/, certs/
- Components page for optional sections (shortcuts, cert import)

### Theme System Overhaul
- **Deleted dark.qss** entirely — replaced by Enterprise Pro as default theme
- Renamed `light.qss` → `enterprise-pro.qss`
- **ThemeManager** enum: `{ EnterprisePro, Classic }`, default = EnterprisePro
- Old "dark" and "light" saved settings auto-migrate to EnterprisePro
- **ThemePalette** (`Core/include/ThemePalette.h`) — central color registry for all widgets
  - Per-theme palettes: bg, panelBg, text, border, accent, success/warning/error, VU colors, deck identity
  - `ThemePalette::forCurrentTheme()` — single call replaces all inline color lookups
  - Registration hook avoids Core→UI circular dependency
- **Stripped 200+ hardcoded `setStyleSheet()` calls** across 45+ files
  - All replaced with ThemePalette lookups or objectName-based QSS rules
  - Fixes theme bleed-through that plagued dark→enterprise-pro switching
- **Font size compliance** — all violations raised to 12px minimum
- **Module dock framing** — visible steel-blue borders, gradient title bars, white card backgrounds on darker surface canvas
- Adding future themes = one new palette in ThemePalette.cpp + one new .qss file

### DeckPlayer Module Redesign
- **Crossfader top-aligned** in center column between Deck A and Deck B
- **PTT panel** flush below crossfader
- **Playlist/AutoDJ permanently embedded** in DeckPlayer center column below crossfader+PTT
  - Auto-created by MainWindow whenever a deck exists, regardless of surface config
  - Fills remaining vertical space between the decks
  - DO NOT REMOVE — this is a permanent architectural decision
- **DeckA/DeckB rich metadata** — loadingFinished now emits full ICY fields (artist, title, album, genre, year, BPM)
- Playlist module added to default configs for alpha, beta, dj, church, company, and custom surfaces

### DeckPlayer Audio Fixes
- **S-Curve crossfade** corrected — equal-power sin/cos prevents clipping at center position
- **Loop boundary guard** — prevents infinite loop when loopOut <= loopIn
- **Ring buffer overflow** — StreamReader uses int64_t indices, safe for weeks of continuous streaming
- **M3U/PLS validation** — validates extracted URLs before passing to StreamReader
- **Resampler flush** — flushes remaining samples on stream disconnect
- **libmp3lame.dll** — CMake post-build auto-copy from `external/lame/bin/` to build output

### Version Bump Cadence
- VERSION.txt at project root tracks current release
- All docs, CHANGELOG, installer, CMakeLists.txt, vcpkg.json bumped in sync

## [0.3.0-alpha] — 2026-03-14

### Phase DB — Database Multi-Backend (5 drivers)
- **DatabaseFactory** — central factory with driver registration pattern
- **SqlDialect** — backend-specific DDL abstraction (SQLite, MySQL, PostgreSQL, Firebird, MSSQL)
- **DbServerEntry::Backend** enum — `{SQLite, MySQL, PostgreSQL, Firebird, MSSQL}` with helpers
- **PostgresManager** — PostgreSQL client via libpq (conditional `M1_HAS_POSTGRESQL`)
- **FirebirdManager** — Firebird client via fbclient (conditional `M1_HAS_FIREBIRD`)
- **MssqlManager** — SQL Server via ODBC (conditional `M1_HAS_MSSQL`, auto-detects best driver)
- **DbServerDialog** — full 5-backend CRUD UI with dynamic port switching
- **PreferencesDialog** DB Servers page — server management + per-surface DB assignments
- **DatabaseModule** (`com.mcaster1.database`) — surface-loadable module with connection status, table browser
- **Per-surface DB isolation** — IDbAwareModule mixin + SurfaceDbContext live wiring
- **DbServerRegistry** signals — `serverListChanged()`, `surfaceAssignmentChanged()`
- **Live DB context push** — changes in Preferences propagate to live modules instantly

### Phase 12 — Installer + Polish + AuxDeck
- **AuxDeckModule** (`com.mcaster1.auxdeck`) — custom auxiliary decks with per-deck audio device routing
  - Custom deck naming (e.g. "Jingle Deck", "Interview Playback")
  - Independent AIR OUT and CUE OUT device selection per deck
  - Volume control + VU meters + full transport (load/play/pause/stop/cue)
  - Mcaster1AudioPipe virtual devices appear automatically in device lists
- **Mcaster1AudioPipe integration** — Preferences > Audio page shows AudioPipe status, launch button, refresh pipe devices
- **Auto version bump** — build number increments on each CMake configure via `cmake/AutoBuildNumber.cmake`
- **Test harnesses** — 6 test executables: TestDatabases, TestCore, TestModules, TestChurch, TestPodcast, TestAuxDeck
- **NSIS installer** — installs to `C:\Users\USERNAME\Mcaster1\Mcaster1Studio` (no admin required)
  - Desktop + Start Menu shortcuts
  - Getting Started guide link
  - Uninstaller with Add/Remove Programs registry
- **Getting Started guide** — `docs/GettingStarted.html`, comprehensive first-run walkthrough
- **3D SVG icon** — `auxdeck.svg` matching project icon style
- Module catalog expanded to 43 built-in modules

### Golden Path — Per-Surface Thread Pools + CPU Affinity
- **ThreadPoolManager** singleton — creates/destroys per-surface `QThreadPool` instances, manages CPU core budget
- **SurfaceThreadPool** — per-surface thread pool wrapper with atomic metrics (submitted/completed/pending/peak)
- **IThreadPoolAware** mixin — opt-in interface for modules needing background thread access (follows `IDbAwareModule` pattern)
- **PooledRunnable** — `QRunnable` wrapper with automatic completion counter
- **CPU affinity** — PortAudio RT callback pinned to core 0, EncoderSlots round-robin cores 1-3 via `SetThreadAffinityMask`
- **5 modules converted:** PodEncodeModule, PodEditorModule, PodTranscribeModule, TranscribeRecModule, MediaLibraryModule
- **HealthSnapshot** extended with per-surface pool metrics (activeThreads, pendingTasks, tasksCompleted, availableCores)
- **Thread budget algorithm** — `max(2, remaining / pools)` with Church/Podcast +1 bonus, respects `QThread::idealThreadCount()`
- **TestThreadPool** — 37 integration tests: pool basics, 100-task submission, metrics, lifecycle, multi-pool, encoder core assignment, thread isolation
- Zero RT violations confirmed across all `onAudioBlock()` implementations

## [0.2.0-alpha] — 2026-03-10

### Phase CH — Church Surface (12 modules)
- **TimerClockModule** — Master clock + multiple named timers with 3D LCD display
- **GraphicsEngineModule** — Theme-based rendering engine for lyrics, scripture, lower thirds
- **LyricsCasterModule** — Song library, arrangement editor, live section transport, auto-advance
- **ScriptureCasterModule** — Bible verse lookup (66 books, 6 translations), sermon queue, live transport
- **AnnounceCasterModule** — Slide manager with date filtering, loop playback, lower third overlay
- **TelePromptModule** — Scrolling script display, mirror mode, speed control, section markers
- **MediaCasterModule** — Video/image playback with playlist, cue points, loop, preview mode
- **StageMonModule** — Stage monitor: worship/sermon/combined views, clock bar, configurable display
- **AudioMixModule** — Virtual mixing console (8ch), 3-band EQ, compressor, recording, VU meters
- **TranscribeRecModule** — Sermon recording + manual transcript editor, WAV/TXT/SRT export
- **SwitchCasterModule** — Program/Preview video switcher, CUT/DISSOLVE/FTB transitions, lower third
- **ServiceRunnerModule** — Service order rundown, 11 segment types, timer integration, JSON templates
- **wireChurchModules()** — Cross-module wiring: GraphicsEngine, SwitchCaster, StageMon, ServiceRunner

### Phase PD — Podcast Surface (13 modules)
- **PodMixerModule** — Podcast mixing console for multi-track recording
- **PodPTTModule** — Podcast-specific push-to-talk with level monitoring
- **PodRecorderModule** — Multi-track podcast recorder with WAV export
- **PodSoundboardModule** — Sound effects board with customizable pads
- **PodFXModule** — Podcast audio effects (noise gate, de-esser, EQ)
- **PodEditorModule** — DAW-style waveform editor with regions, markers, zoom
- **PodEncodeModule** — Podcast encoding to MP3/AAC/Opus with metadata
- **PodTranscribeModule** — Episode transcription with speaker labels
- **PodShowNotesModule** — Rich text show notes editor with timestamps
- **PodRSSModule** — RSS 2.0 + iTunes namespace feed generator
- **PodPublisherModule** — Multi-platform podcast distribution
- **PodAnalyticsModule** — Download/listener analytics dashboard
- **PodRemoteModule** — Remote guest connection manager
- 13 new 3D SVG icons (pod-*.svg) matching project icon style

### Phase I — Metadata Flow + VU Meters
- Rich ICY metadata push from both Deck A and Deck B
- Encoder VU meter panel with per-slot peak indicators
- Deck VU meter redesign with stereo bars

### Phase H — Audio Engine + SQLite DB
- IDatabase abstraction (SQLite default, MySQL/MariaDB enterprise)
- HTTP stream playback via FFmpeg + ICY 1.x/2.2 metadata receive
- CartWall module with hot-start audio pads

### Phase 11 — SDK + Public Plugin System
- ModuleRegistry built-in catalog with 40+ modules
- IModuleHost interface for plugin access to Studio services
- Dynamic "Add Module" menus per surface
- SampleModule + SampleEffect reference implementations

### Phases 1-10 + A-G — Foundation through UI Polish
- Complete broadcast automation suite: dual-deck player, media library,
  7-codec encoder (MP3/Opus/Vorbis/FLAC/AAC-LC/HE-v1/HE-v2),
  effects rack, ICY 2.2 metadata, playlist/AutoDJ, podcast recording,
  video module, server monitor
- 9 surface types: Alpha, Beta, Company, DJ, Entertainment, Social, Podcast, Church, Custom
- Session persistence v2, sub-surface tabs, theme system (dark/classic/light)
- 3D SVG icon set, SurfaceTray navigation, drag-and-drop module layout
