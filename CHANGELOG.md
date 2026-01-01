# Changelog — Mcaster1Studio

All notable changes to Mcaster1Studio are documented in this file.

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
