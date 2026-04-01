# Changelog — Mcaster1Studio

All notable changes to Mcaster1Studio are documented in this file.

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
