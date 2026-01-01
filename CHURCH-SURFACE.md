# Mcaster1Studio Church Surface and Module Architecture Planning Prompt

## Context for Claude Code

We are building Mcaster1Studio v0.1.0, a Broadcast Automation Studio. This is a desktop application that uses a surface based UI architecture instead of traditional multiple desktops or floating windows. Each surface is an isolated workspace with its own ON AIR / OFF AIR state. Surfaces contain modules that can be arranged, docked, and configured independently within their parent surface. When a surface is OFF AIR, nothing inside it touches the broadcast output. When a surface goes ON AIR, its active modules feed into the broadcast pipeline. This isolation model is the core architectural concept of the entire application.

The application already has working surfaces for live DJ mixing (Surface DJ) and a Player Deck surface. We are now expanding the surface and module system to cover the full range of broadcast automation use cases including live streaming, playlist management, and church production.

Write all code comments in first person plural. Example: "We will build this," "We are using this," "We can do this or that."

All code must be complete, functional, production ready, and fully documented. No placeholder code. No partial snippets. No third party PHP dependencies. Raw SQL only for any database interactions. Every file must include filename, file path from app root, author, date, title, purpose, reason, and a changelog section in the comments.

---

## Pre-Configured Surfaces

Mcaster1Studio ships with the following pre-configured surfaces. Each surface is a self contained workspace that groups related modules together for a specific broadcast workflow.

### Surface 1: Surface DJ (Existing)

Purpose: Live DJ mixing and on air broadcast control.

This surface is already built and functional. It contains dual decks (Deck A and Deck B) with full transport controls, BPM detection, pitch adjustment, volume and pan sliders, per deck AIR and CUE routing, a crossfader with STD/S-CRV/EXP curve modes and configurable fade time, a playlist module with AutoDJ and smart artist/title separation rules, a media library with search and scan directory functionality, and per deck EQ and four aux send knobs (H1 through H4).

### Surface 2: Surface Live Stream

Purpose: Dedicated live streaming broadcast control.

This surface focuses on encoding and stream output management. It should contain stream encoder configuration and status for multiple simultaneous outputs (up to 8 encoder slots as shown in the existing status bar), stream health monitoring with bitrate, dropped frames, and connection status, metadata management for updating now playing information across all active streams, listener statistics and connection tracking, and stream recording controls for archiving the broadcast output to file.

### Surface 3: Surface Planner

Purpose: Playlist management, rotation planning, and scheduling.

This surface is the offline planning workspace where the station operator builds playlists, configures rotation rules, schedules programming blocks, and manages the media library. It should contain a rotation rule builder for defining how tracks are selected (genre weights, dayparting, artist separation, energy level curves), a schedule grid for programming blocks across the broadcast week, playlist import and export functionality, media library management with batch tagging and metadata editing, and clock wheel visualization for seeing the hour by hour rotation pattern.

### Surface 4: Surface Church

Purpose: Full church service production including worship presentation, sermon support, audio/video production, and live streaming.

This is the new surface we are planning. It contains the following twelve modules defined below.

---

## Church Surface Module Definitions

### Module 1: M1S-GraphicsEngine

Full Name: Mcaster1Studio Graphics Engine
Purpose: Template and overlay rendering backend.
Priority: Foundation layer. Must be built first because LyricsCaster, ScriptureCaster, AnnounceCaster, and TelePrompt all depend on it.

This is the rendering engine that all visual output modules pull their templates from. It handles title card rendering with church branding (logo, color scheme, fonts), background image and video loop management for behind text overlays, lower third template rendering with animated transitions, scripture reference formatting templates, text layout engine for handling multi-line lyrics, verse references, and sermon notes with proper line breaking and overflow handling, theme management so the church can define multiple visual themes (Christmas, Easter, standard Sunday, special events) and switch between them, and output resolution management for targeting different display sizes (projector, confidence monitor, stream overlay).

The GraphicsEngine does not display anything on its own. It provides rendering services to the other modules that need visual output.

### Module 2: M1S-LyricsCaster

Full Name: Mcaster1Studio Lyrics Caster
Purpose: Worship lyrics display for congregation projection.
Depends On: M1S-GraphicsEngine for visual rendering.

This module handles the display of worship song lyrics on the main projector during the worship set. It should support song library management with verse, chorus, bridge, pre-chorus, and tag section types, manual operator advance (next section, previous section, go to specific section), optional auto-advance with configurable timing per section, blank/black screen toggle for moments of prayer or reflection, song arrangement builder so the worship leader can define the order of sections for each song (verse 1, chorus, verse 2, chorus, bridge, chorus, tag), live section reordering during service for when the worship leader calls an audible, key and tempo notation per song for the worship team reference, and CCLI license number display for copyright compliance.

The operator view shows the current section, next section preview, and the full song arrangement with position indicator. The congregation view (projector output) shows only the current lyrics rendered through the GraphicsEngine templates.

### Module 3: M1S-ScriptureCaster

Full Name: Mcaster1Studio Scripture Caster
Purpose: Bible verse and sermon notes display for congregation projection.
Depends On: M1S-GraphicsEngine for visual rendering.

This module handles displaying Bible verses and sermon outline points on the main projector during the sermon. It should include a built in Bible text database supporting multiple translations (KJV, NIV, ESV, NASB, NLT, NKJV at minimum), quick lookup by book, chapter, and verse with type ahead search (pastor types "John 3:16" and the full text populates automatically), passage range support (John 3:16-18 displays all three verses), split screen capability for showing two translations side by side, sermon outline point display with hierarchical numbering (I, A, 1, a format), pastor controlled content queue where the pastor pre-loads his scripture references and outline points in sermon order, and manual advance through the queue during the sermon.

The operator view shows the full sermon queue with current position. The congregation view shows the current verse or outline point rendered through the GraphicsEngine.

### Module 4: M1S-AnnounceCaster

Full Name: Mcaster1Studio Announce Caster
Purpose: Announcements, lower thirds, and pre/post service information display.
Depends On: M1S-GraphicsEngine for visual rendering.

This module handles all non-worship, non-sermon visual content. It should support announcement slide creation with text and image placement, pre-service loop mode that cycles through announcements on a timer before service starts, transition service slides (welcome, offering, closing), lower third overlays for identifying speakers, displaying giving information, or showing social media handles, QR code generation and display for giving links, event signups, and visitor connection cards, countdown timer display for pre-service and between segments, and scheduling so announcements can be assigned to specific services or date ranges and automatically expire.

### Module 5: M1S-MediaCaster

Full Name: Mcaster1Studio Media Caster
Purpose: Video and media playback for service content.
Depends On: M1S-GraphicsEngine for overlay integration.

This module handles all video and pre-produced media playback during the service. It should support video file playback (MP4, MOV, AVI, WebM at minimum), audio file playback for backing tracks or special music, pre-service countdown video support, cue point marking for precise start and stop points within a media file, preview mode so the operator can check media before sending it to the projector, volume control independent from the main audio mix, and alpha channel support for overlay videos (animated backgrounds, motion graphics).

### Module 6: M1S-SwitchCaster

Full Name: Mcaster1Studio Switch Caster
Purpose: Video source switching and output routing.
Depends On: All visual modules feed into this as sources.

This module is the video router that controls what goes to which output. It should support multiple input sources including camera feeds, LyricsCaster output, ScriptureCaster output, AnnounceCaster output, MediaCaster output, and any external video input, scene presets that define specific source layouts (full screen camera, picture in picture with lyrics over camera, side by side split), transition effects between scenes (cut, dissolve, fade to black), multiple output destinations (main projector, stage monitors, live stream, recording), preview bus so the operator can see what a scene looks like before taking it live, and program bus showing what is currently live on each output.

### Module 7: M1S-StageMon

Full Name: Mcaster1Studio Stage Monitor
Purpose: Confidence display output for pastor and worship team.
Depends On: M1S-LyricsCaster, M1S-ScriptureCaster, M1S-TimerClock.

This module generates a separate output feed for the stage facing displays. The pastor confidence monitor shows the current sermon slide or scripture reference, preview of the next item in the queue, current time, sermon elapsed timer, and optionally the sermon notes or teleprompter text. The worship team monitor shows current lyrics with the current section highlighted, next section preview, song arrangement position, and key and tempo reference. The stage monitor output is independent from the congregation projector output so the stage team always sees more context than the audience.

### Module 8: M1S-TelePrompt

Full Name: Mcaster1Studio TelePrompter
Purpose: Scrolling script display for pastor, worship leader, or announcer.
Depends On: M1S-GraphicsEngine for text rendering.

This module provides a teleprompter display for anyone who needs to read from a script during the service. It should support script import from text files, Word documents, or direct text entry, adjustable scroll speed with smooth acceleration and deceleration, adjustable font size and color for readability at distance, mirror mode for beam splitter prompter hardware rigs, remote control support so the pastor or a dedicated operator can control scroll speed and position, pause and resume with position memory, section markers for jumping to specific points in the script, and dual mode where it can show the full sermon script or just the current outline point depending on preference.

### Module 9: M1S-TranscribeRec

Full Name: Mcaster1Studio Transcription Recorder
Purpose: Sermon audio recording and speech to text transcription.
Depends On: Existing audio engine and PTT mic pipeline.

This module captures the pastor's sermon audio and converts it to text. It should support dedicated recording channel that captures only the pastor mic (isolated from worship band audio), multiple output format recording (WAV for archival, MP3 for web publishing), automatic file naming with date, service type, and sermon title, real time speech to text transcription during the sermon, post-recording transcription for higher accuracy processing after the service, transcript editing interface for correcting transcription errors, export formats including plain text, Word document, and SRT subtitle format, timestamp markers in the transcript synced to the audio recording for easy navigation, and integration with the ServiceRunner so recording starts and stops automatically based on the sermon segment in the rundown.

### Module 10: M1S-AudioMix

Full Name: Mcaster1Studio Audio Mixer
Purpose: Live service audio mixing and recording.
Depends On: Existing audio engine infrastructure.

This module handles the full service audio mix. It should support multi-channel input configuration for pastor mic, worship vocals, instruments, and media playback, per channel volume, pan, mute, and solo controls, per channel processing chain (gate, EQ, compressor, de-esser), master bus processing for the final mix, aux send buses for monitor mixes (stage monitors, in-ear monitors), main mix recording to file for full service archival, separate recording buses for isolating individual channels or submixes, and integration with the existing deck audio engine so media playback from MediaCaster routes through the mixer.

### Module 11: M1S-ServiceRunner

Full Name: Mcaster1Studio Service Runner
Purpose: Master service planner and rundown orchestration engine.
Priority: Orchestration layer. This is the module that ties every other Church Surface module together.
Depends On: All other Church Surface modules.

This is the backbone of the Church Surface. It provides a timeline based rundown of the entire service where each item links to the appropriate module and content. A typical service rundown would look like this:

1. Pre-Service Countdown (triggers AnnounceCaster loop and TimerClock countdown)
2. Welcome (triggers MediaCaster welcome video or AnnounceCaster welcome slide)
3. Worship Song 1 (triggers LyricsCaster with specific song, SwitchCaster to worship camera scene)
4. Worship Song 2 (triggers LyricsCaster with next song, transitions automatically)
5. Worship Song 3 (triggers LyricsCaster with next song)
6. Offering (triggers AnnounceCaster with giving slide and QR code, MediaCaster with offering video)
7. Sermon Introduction (triggers ScriptureCaster with opening verse, SwitchCaster to pastor camera)
8. Sermon (triggers ScriptureCaster queue, TelePrompt script, TranscribeRec recording start, TimerClock sermon timer)
9. Altar Call (triggers LyricsCaster with response song, SwitchCaster to wide shot)
10. Closing (triggers AnnounceCaster with closing slides, LyricsCaster with closing song)
11. Dismissal (triggers AnnounceCaster with post-service loop)

The ServiceRunner should support drag and drop rundown building, one click advance to next item (the AV operator hits "next" and all linked modules update simultaneously), manual override to jump to any item in the rundown, per item module assignments (which modules activate and what content they load), per item timing (expected duration for keeping the service on schedule), rundown templates that can be saved and reused (standard Sunday, Christmas Eve, Easter, midweek), live progress tracking showing current item, elapsed time, and schedule variance, and rehearsal mode that walks through the rundown without going live.

### Module 12: M1S-TimerClock

Full Name: Mcaster1Studio Timer and Clock
Purpose: Master clock and timer system.
Depends On: None. This is a shared service.

This module provides all time related data to every other module. It should support a master clock display synced to NTP for accuracy, service countdown timer (counts down to service start time and displays on projector via AnnounceCaster), sermon timer (starts when sermon segment begins in ServiceRunner, visible to pastor on StageMon), segment timers for tracking duration of each rundown item, alarm and warning thresholds (sermon timer turns yellow at 5 minutes remaining, red at 0), multiple simultaneous timer instances, and time of day display for the status bar and any module that needs current time.

---

## Module Dependency Map

```
Foundation Layer:
  M1S-GraphicsEngine (rendering backend)
  M1S-TimerClock (time services)

Visual Output Modules (depend on GraphicsEngine):
  M1S-LyricsCaster
  M1S-ScriptureCaster
  M1S-AnnounceCaster
  M1S-TelePrompt

Audio Modules (depend on existing audio engine):
  M1S-AudioMix
  M1S-TranscribeRec

Routing Layer (consumes all visual modules):
  M1S-SwitchCaster

Display Output (consumes visual modules and TimerClock):
  M1S-StageMon

Media Playback:
  M1S-MediaCaster

Orchestration Layer (commands all modules):
  M1S-ServiceRunner
```

## Build Order Recommendation

Phase 1: Foundation
1. M1S-TimerClock (no dependencies, shared service)
2. M1S-GraphicsEngine (rendering backend everything visual needs)

Phase 2: Core Visual Modules
3. M1S-LyricsCaster (most used module in church context)
4. M1S-ScriptureCaster (second most used)
5. M1S-AnnounceCaster (pre/post service content)

Phase 3: Production Modules
6. M1S-TelePrompt (pastor facing tool)
7. M1S-MediaCaster (video playback)
8. M1S-StageMon (stage confidence displays)

Phase 4: Audio and Recording
9. M1S-AudioMix (live mixing)
10. M1S-TranscribeRec (sermon recording and transcription)

Phase 5: Routing and Orchestration
11. M1S-SwitchCaster (video routing, needs all sources built first)
12. M1S-ServiceRunner (orchestration, needs all modules built first)

---

## Surface Architecture Technical Notes

Each surface in Mcaster1Studio operates as an isolated container. Surfaces do not share state with other surfaces unless explicitly routed through the audio engine or video output pipeline. Each surface has its own ON AIR / OFF AIR toggle that controls whether its output reaches the broadcast pipeline. Modules within a surface can be rearranged, resized, and configured independently. Each module has a consistent title bar with minimize, maximize, detach, info, and close controls. The bottom status bar is global across all surfaces and shows audio engine status, encoder count, now playing metadata, and the master clock.

The surface tab bar at the top of the application shows all available surfaces with their current ON AIR / OFF AIR status. Only one surface is visible at a time in the main viewport, but multiple surfaces can be ON AIR simultaneously. This means the DJ surface can be broadcasting music while the Church surface is being set up for the next service, and they will not interfere with each other until the operator explicitly switches which surface is feeding the broadcast output.

---

## Existing Application Reference

The application title bar format is: Mcaster1Studio v0.1.0 — Broadcast Automation Studio

The global header contains: station identifier (e.g., GRR BROADCAST), digital clock display, location name, current temperature, and global ON AIR control.

The status bar format is: Audio: [sample rate] Hz | [buffer size] frames/buf | Now: [artist] - [title] | Enc: [active]/[total] | Engine: [status] | [clock] [date]

The bottom tab bar provides quick access to all modules across all surfaces: TRAY, LOG, SCHED, PTT Mic, Media Library, Playlist, Queue, Deck A, Deck B, and additional module specific tabs.