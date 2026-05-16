# Mcaster1Studio AI Integration — Features & Status

**Version:** 0.4.0-beta | **Date:** 2026-03-18 | **Overall Status:** ~35% complete (in testing/QA)

---

## Overview

Mcaster1Studio integrates AI capabilities across the broadcast workflow — from music recommendations and playlist generation to artist research and on-air persona management. The AI system is provider-agnostic, supporting six backends, and operates through a persona framework that tailors AI behavior to the station's format, time of day, and surface context.

Current status: approximately 35% of planned AI features are implemented and in active testing/QA. Core infrastructure (provider abstraction, persona system, artist intel, category AI tools) is functional. Streaming token support, advanced AuxDeck routing, and some animated UI elements are upcoming.

---

## AI Providers

Mcaster1Studio supports six AI provider backends. The user configures their preferred provider and API key in **Preferences > AI**.

| Provider | Type | Notes |
|----------|------|-------|
| **Ollama** | Local / self-hosted | No API key required; runs on local hardware |
| **Claude** | Cloud (Anthropic) | API key required; supports Claude 3+ models |
| **ChatGPT** | Cloud (OpenAI) | API key required; supports GPT-4o and later |
| **Grok** | Cloud (xAI) | API key required |
| **Gemini** | Cloud (Google) | API key required; supports Gemini 1.5+ |
| **Venice** | Cloud (Venice.ai) | API key required; privacy-focused provider |

All providers use a common request/response abstraction so that switching providers does not affect feature behavior.

---

## AI Persona System

15 preset AI personas ship with Mcaster1Studio, covering the full range of broadcast formats:

### Radio DJ Personas (7)
- **Top 40 / CHR** — High energy, trend-focused, pop culture savvy
- **Classic Rock** — Storytelling, deep cuts, artist history
- **Country** — Warm, community-focused, Nashville insider
- **Urban / Hip-Hop** — Culture-forward, slang-aware, mixtape curator
- **Jazz / Blues** — Sophisticated, smooth delivery, session musician knowledge
- **EDM / Dance** — Festival energy, BPM-aware, remix knowledge
- **Adult Contemporary** — Polished, relatable, easy listening expertise

### Podcast Personas (3)
- **Interview Host** — Conversational, question-driven, guest-focused
- **Solo Narrator** — Storytelling, pacing-aware, chapter structure
- **Panel Moderator** — Multi-voice balance, topic steering, time management

### Specialty Personas (5)
- **Sports Broadcaster** — Stats-driven, play-by-play energy, team knowledge
- **TV News Anchor** — Authoritative, concise, breaking-news cadence
- **Church / Worship** — Reverent, scripture-aware, service flow sensitive
- **Social Media Host** — Casual, meme-literate, engagement-focused
- **Producer / Behind the Scenes** — Technical, workflow-oriented, gear-savvy

### Key Features
- **3-tier resolution:** daypart schedule > surface-level persona > global default
- **Per-category assignment:** right-click any library category to assign a persona
- **Editable system prompt:** each persona has a system prompt (500 char max) that users can customize
- **Database tables:** `ai_personas` and `daypart_schedule` tables exist in all 5 SQL dialects (SQLite, MySQL, PostgreSQL, Firebird, MSSQL)
- **PersonaManager** singleton handles resolution logic and caching
- **Preferences > AI** tab: persona combo selector + system prompt editor

---

## Artist Intel Dialog

A full-window research dialog accessible via right-click on any track in the Media Library.

### Tabs
- **Overview** — Summary bio, genre, active years, notable works
- **Discography** — Albums, singles, compilations with release dates
- **Images** — Artist photos and album artwork (cache-backed)
- **DJ Script** — Ready-to-read on-air talking points

### 8 AI Research Buttons
1. **Touring History** — Concert tours, notable venues, tour companions
2. **Musical Influences** — Who influenced the artist and who they influenced
3. **Awards & Charts** — Grammy wins, Billboard peaks, certifications
4. **Broadcaster Script** — Pre-written DJ patter for on-air use
5. **Fan Base & Impact** — Cultural significance, fan demographics, legacy
6. **Full Timeline** — Chronological career events
7. **Gear & Equipment** — Instruments, studios, production tools
8. **Discovery** — How the artist was discovered, early career breaks

### Interaction Features
- **Multi-turn "Ask AI" chat** with full conversation context maintained
- **Save/load per-tab** as JSON — overview and research tabs persist independently
- **Right-click tab** context menu: Refresh Report, Save Individual Report as HTML
- **Auto-retry on busy** — 5-second delay with user-friendly error messages
- **Save confirmation dialog** showing database path, table name, and record ID
- **AI Intel badges** appear in the library track list (ColIntel column) after saving research

---

## AI DJ Agent Browser

An interactive conversational recommendation engine accessible from the Media Library's category view.

- **Persona-aware** — recommendations reflect the active AI persona's style and format
- **Conversational interface** — multi-turn chat where the DJ can ask for recommendations, explain preferences, and refine results
- **Category-scoped** — recommendations draw from the active library category
- **Drag-to-deck** — recommended tracks can be dragged directly to Deck A/B or the playlist queue
- **Animated braille spinner** progress indicator with phase messages during AI processing

---

## Playlist Generator Pro

AI-powered playlist generation with fine-grained control over output.

- **Source selector** — choose which library categories to draw from
- **AI Intel prioritization** — tracks with saved AI research are weighted higher in selection
- **Broadcast element insertion** — automatically insert station IDs, sweepers, jingles, and ads at configurable intervals
- **Duration targeting** — generate playlists for specific time blocks (1h, 2h, 4h, custom)
- **Persona influence** — the active persona shapes track selection, ordering, and element placement
- **Animated braille spinner** with phase messages (Analyzing library, Selecting tracks, Arranging flow, Inserting elements)

---

## Daypart Scheduler Pro

A 24-hour visual timeline for scheduling AI personas across the broadcast day.

- **24h timeline** — visual hour-by-hour grid showing which persona is active
- **Persona per time block** — assign different personas to morning drive, midday, afternoon, evening, overnight
- **AI auto-schedule** — let the AI suggest a daypart layout based on station format and target audience
- **Per-surface scheduling** — each surface can have its own daypart schedule
- **Database-backed** — schedules persist in the `daypart_schedule` table across all 5 SQL backends
- **Drag-to-resize** time blocks in the visual editor

---

## Media Library AI Features

AI capabilities integrated directly into the Media Library module.

- **FTS5 full-text search** — SQLite FTS5 virtual table for fast search; falls back to LIKE queries if FTS5 extension is unavailable
- **AI Intel badges** — tracks with saved Artist Intel research display a badge in the ColIntel column
- **Category management** — 7 preset categories (Music, Stingers, Station IDs, Sweepers, Jingles, Ads, Spoken Word) with hierarchical subcategories (3+ levels)
- **Per-category persona assignment** — right-click category to assign an AI persona that governs recommendations within that category
- **Scan-into-category** — folder scanning can auto-assign tracks to categories based on path or metadata
- **FTS5 search scoped to category** — searching within a selected category restricts results to that category's tracks
- **Album art cache** — memory + disk cache with TagLib extraction for cover art display

---

## Upcoming

Features planned for near-term development:

- **AuxDeck 4-bus routing** — route AuxDeck output to 4 independent buses (AIR, CUE, REC, MON) for advanced monitoring workflows
- **Animated progress indicators** — expanding braille spinner animations to all long-running AI operations across the application
- **Streaming token support** — support for streaming/chunked AI responses with real-time token display, reducing perceived latency for long AI operations
- **AI-powered show prep** — automated pre-show research packets combining artist intel, trending topics, and daypart context
- **Voice cloning integration** — AI voice synthesis for pre-recorded liners and sweepers using station voice talent samples
