# Mcaster1Studio Podcast Surface and Module Architecture Planning Prompt

## Context for Claude Code

We are building Mcaster1Studio v0.1.0, a Broadcast Automation Studio. This is a desktop application that uses a surface based UI architecture instead of traditional multiple desktops or floating windows. Each surface is an isolated workspace with its own ON AIR / OFF AIR state. Surfaces contain modules that can be arranged, docked, and configured independently within their parent surface. When a surface is OFF AIR, nothing inside it touches the broadcast output. When a surface goes ON AIR, its active modules feed into the broadcast pipeline. This isolation model is the core architectural concept of the entire application.

The application already has working surfaces for live DJ mixing (Surface DJ) and a Player Deck surface. We have also planned out Surface Live Stream, Surface Planner, and Surface Church with twelve modules. We are now planning Surface Podcast, which is a self contained mini podcast production studio within Mcaster1Studio.

Write all code comments in first person plural. Example: "We will build this," "We are using this," "We can do this or that."

All code must be complete, functional, production ready, and fully documented. No placeholder code. No partial snippets. No third party PHP dependencies. Raw SQL only for any database interactions. Every file must include filename, file path from app root, author, date, title, purpose, reason, and a changelog section in the comments.

---

## Surface 5: Surface Podcast

Purpose: Self contained podcast production studio. Record, mix, edit, process, encode, publish, and distribute podcast episodes entirely within a single surface. The Podcast Surface gives podcasters a complete production environment that replaces the need for separate DAW software, encoding tools, upload clients, and RSS feed management platforms.

The design philosophy for this surface is that a podcaster should be able to sit down, open the Podcast Surface, record their episode, mix and master it, encode it, publish it to their hosting server, generate the RSS feed entry, and distribute it to podcast directories without ever leaving Mcaster1Studio.

---

## Podcast Surface Module Definitions

### Module 1: M1S-PodMixer

Full Name: Mcaster1Studio Podcast Mixer
Purpose: Six channel desktop mixer interface that emulates a real Mackie style hardware mixer.
Priority: Foundation audio module for the Podcast Surface. Everything else depends on this for audio routing.

This is the centerpiece of the Podcast Surface. It should look and feel like a real six channel desktop mixer with physical slider style fader controls, not just simple volume bars. The visual design should emulate the layout and feel of a Mackie 802VLZ4 or similar compact analog mixer where each channel strip is a vertical column with clearly defined sections from top to bottom.

Each of the six channel strips must include the following from top to bottom in the visual layout: an input source selector (microphone input, line input, USB audio device, internal audio bus, media file playback, or remote guest feed), a gain/trim knob at the top of the strip for input level adjustment, a three band EQ section with high, mid, and low frequency knobs per channel, two aux send knobs (AUX 1 and AUX 2) for routing to effects buses or monitor mixes, a pan knob for stereo positioning, a solo button (S) that isolates the channel to the headphone bus for monitoring, a mute button (M) that silences the channel from the main mix, a channel level meter (vertical LED style bar showing signal level with green, yellow, and red zones), a motorized style vertical fader slider (the main volume control for the channel, long throw style), and a channel label field at the bottom that the user can rename (Host, Co-Host, Guest 1, Guest 2, Sound FX, Music Bed).

The master section on the right side of the mixer must include a master stereo fader pair (left and right linked), master level meters (stereo VU or LED style), master mute, a headphone volume knob with source selector (main mix, solo bus, cue bus), a monitor output volume knob, and a record level meter showing the signal going to the recorder.

The mixer must also have two aux return channels for bringing processed effects back into the mix, a built in insert point architecture on each channel for inline processing (compressor, gate, de-esser, EQ), and a signal flow that routes the final master output to the M1S-PodRecorder for recording and to the stream encoder if live podcasting.

The visual design should use dark grey and charcoal colors with the classic colored knob caps (red for EQ high, yellow for EQ mid, green for EQ low, blue for aux sends, grey for pan) that Mackie mixers are known for. Fader caps should be the traditional T-bar shape. Channel meters should animate in real time with signal levels.

### Module 2: M1S-PodRecorder

Full Name: Mcaster1Studio Podcast Recorder
Purpose: Multi-track audio recording engine for capturing podcast episodes.
Depends On: M1S-PodMixer for audio input routing.

This module handles the actual recording of the podcast episode. It should support multi-track recording where each mixer channel is captured to its own isolated track file simultaneously with the stereo master mix, configurable recording formats (WAV 16-bit/24-bit at 44.1kHz or 48kHz for archival quality, MP3 at configurable bitrates for quick drafts), automatic file naming with show name, episode number, date, and take number, punch-in and punch-out recording for fixing mistakes without re-recording the entire episode, pre-roll and post-roll configurable silence padding, marker/cue point insertion during recording so the podcaster can flag edit points in real time while recording (press a hotkey to drop a marker at the current timestamp), take management so the podcaster can do multiple takes and select the best one, recording timer showing elapsed time, remaining disk space estimate, and file size, auto-save with configurable interval so a crash does not lose the entire recording, and a record-ready arming system per channel so the podcaster can choose which channels are being captured.

The recorder receives its audio from the PodMixer master output for the stereo mix and from individual channel direct outs for multitrack isolation.

### Module 3: M1S-PodEditor

Full Name: Mcaster1Studio Podcast Editor
Purpose: Non-destructive audio editing for post-production of recorded episodes.
Depends On: M1S-PodRecorder for source audio files.

This is the post-production editing module. After recording, the podcaster uses this to clean up and assemble the final episode. It should support a multi-track waveform timeline display showing all recorded tracks visually, non-destructive editing (all edits are reversible, original files are never modified), cut, copy, paste, delete, split, and trim operations on audio regions, crossfade tools for smooth transitions between edits, silence detection and removal with configurable threshold and minimum duration, filler word detection highlighting (um, uh, like, you know, so, basically) with one click removal, noise floor analysis and noise reduction processing, volume envelope automation (draw volume curves over time for ducking music under voice), time stretching for minor timing adjustments without pitch change, undo history with unlimited levels, ripple editing mode where deleting a section automatically closes the gap, and multi-track alignment tools for syncing tracks that were recorded on separate devices.

The editor should display the cue markers that were placed during recording so the podcaster can quickly jump to flagged edit points.

### Module 4: M1S-PodFX

Full Name: Mcaster1Studio Podcast Effects Rack
Purpose: Audio processing effects chain for podcast voice and mix treatment.
Depends On: M1S-PodMixer for real time insert processing, M1S-PodEditor for offline processing.

This module provides a virtual effects rack that can be applied either in real time through the mixer insert points or as offline processing during editing. The rack should include the following built in processors: a noise gate with threshold, attack, hold, and release controls for cutting background noise between speech, a compressor with threshold, ratio, attack, release, and makeup gain controls for evening out vocal dynamics, a de-esser with frequency, threshold, and reduction controls for taming sibilance (harsh S and T sounds), a parametric EQ with four bands (low shelf, low-mid bell, high-mid bell, high shelf) each with frequency, gain, and Q controls, a high-pass filter with adjustable cutoff frequency for removing low frequency rumble (HVAC, traffic, handling noise), a limiter with ceiling and release controls for preventing clipping on the master bus, a de-reverb processor for reducing room echo and reflections in untreated recording spaces, a stereo enhancer for widening or narrowing the stereo image of the master mix, and a loudness meter displaying LUFS (Loudness Units relative to Full Scale) with target level indicators for podcast standards (typically -16 LUFS for stereo, -19 LUFS for mono per Apple and Spotify specs).

Each processor should have a bypass toggle, wet/dry mix control, and preset management. The effects rack should support drag and drop reordering of the processing chain. The visual design for each processor should emulate the look of real outboard rack gear with knobs, buttons, and meters.

### Module 5: M1S-PodPTT

Full Name: Mcaster1Studio Podcast Push-to-Talk
Purpose: Enhanced PTT module specifically designed for podcast workflows.
Depends On: M1S-PodMixer for audio routing.

This is an improved version of the existing PTT Mic module tailored specifically for podcast production. The existing PTT module has a basic gate, de-esser, and compressor. The PodPTT module extends this with a full inline processing chain: noise gate (same as existing), de-esser (same as existing but with visual frequency display), compressor (same as existing but with gain reduction meter), high-pass filter for rumble removal, and a loudness normalizer.

Additional features beyond the existing PTT module include a cough button (momentary mute that silences the mic without the click sound of a hard mute toggle, implemented as a ducking fade rather than a hard cut), talkback functionality where the host can speak to guests through their headphones without it going to the recording or stream, a voice activity indicator (LED style light that shows when the mic is picking up speech versus silence for visual confirmation that the mic is working), microphone selection with device enumeration and refresh, sample rate and buffer size configuration per mic input, input monitoring toggle (hear yourself in headphones with zero or near-zero latency), and configurable PTT behavior (hold-to-talk, toggle-on/toggle-off, or always-on modes).

The PodPTT module should also support multiple instances so each podcast participant can have their own PodPTT module configured for their specific mic and processing preferences.

### Module 6: M1S-PodEncode

Full Name: Mcaster1Studio Podcast Encoder
Purpose: Audio encoding and format conversion for podcast distribution.
Depends On: M1S-PodEditor or M1S-PodRecorder for source audio.

This module handles encoding the finished podcast episode into distribution-ready formats. It should support the following output formats: MP3 (CBR and VBR modes, bitrates from 64kbps to 320kbps, joint stereo and true stereo modes), AAC/M4A (the preferred format for Apple Podcasts, with bitrate configuration), OGG Vorbis (for open format distribution and Mcaster1DNAS/Icecast compatibility), FLAC (for lossless distribution), and Opus (for high efficiency at low bitrates).

Encoding configuration should include sample rate selection (22050, 44100, 48000 Hz), channel mode (mono, stereo, joint stereo), bitrate selection or quality level, and loudness normalization to target LUFS during encoding.

The encoder must also handle ID3 tag embedding for MP3 and equivalent metadata embedding for other formats. Tag fields should include: podcast show title, episode title, episode number, season number, author/host name, description/show notes, genre (always "Podcast"), publication date, cover art image (embedded JPEG or PNG, minimum 1400x1400, maximum 3000x3000 per Apple spec), copyright notice, URL link to the podcast website, and custom tags for any additional metadata.

Batch encoding support so the podcaster can encode one source file into multiple formats simultaneously (for example, MP3 for general distribution plus AAC for Apple plus OGG for Icecast) in a single operation.

### Module 7: M1S-PodPublisher

Full Name: Mcaster1Studio Podcast Publisher
Purpose: Publishing and uploading encoded podcast episodes to hosting servers.
Depends On: M1S-PodEncode for encoded files, M1S-PodRSS for feed updates.

This module handles the actual distribution of the encoded episode file to the podcast hosting infrastructure. It supports multiple publishing targets that can be configured and saved as profiles.

Publishing target types include: ICY 2.2 publishing to Mcaster1DNAS server (our own DNAS platform using the ICY 2.0/2.2 protocol for pushing podcast content to the server, including authentication, metadata push, and content upload via the DNAS API), Icecast2 source publishing (standard Icecast2 source client connection for pushing audio content to an Icecast2 server), SFTP upload (Secure FTP upload to a remote server with configurable host, port, username, key or password authentication, and remote directory path), SCP upload (Secure Copy Protocol for direct file transfer over SSH), SSH/RSYNC upload (for incremental file synchronization to a remote server, efficient for updating large media libraries), HTTP/HTTPS POST upload (REST API based publishing for modern podcast hosting platforms that accept file uploads via HTTP), and FTP/FTPS upload (legacy FTP support with optional TLS encryption for older hosting setups).

Each publishing profile should store: connection credentials (stored securely, encrypted at rest), remote directory path, naming convention for uploaded files, post-upload verification (file size check, checksum verification), and automatic retry with configurable attempt count and backoff interval.

The publisher should support publishing to multiple targets simultaneously (for example, upload the MP3 to the web server via SFTP, push the OGG to Mcaster1DNAS via ICY 2.2, and POST to a REST API for the podcast hosting platform, all in one operation).

Publishing status should show per-target progress with transfer speed, percentage complete, and estimated time remaining.

### Module 8: M1S-PodRSS

Full Name: Mcaster1Studio Podcast RSS Feed Manager
Purpose: RSS 2.0 podcast feed generation, management, and distribution to podcast directories.
Depends On: M1S-PodEncode for episode metadata, M1S-PodPublisher for file URLs.

This module generates and manages the podcast RSS feed that directories like Apple Podcasts, Spotify, Google Podcasts, Amazon Music, and others consume to list the show and its episodes. The RSS feed must comply with RSS 2.0 specification, Apple iTunes podcast namespace extensions, and the Podcast Standards Project (PSP) recommendations.

Show level (channel) configuration includes: podcast title, podcast description (short and long versions), podcast author and owner name/email, podcast category and subcategory (matching Apple Podcasts taxonomy), podcast cover art URL (minimum 1400x1400, maximum 3000x3000 JPEG or PNG), podcast website link, podcast language code (en-us, etc.), explicit content flag (yes/no/clean), podcast type (episodic or serial), copyright notice, podcast GUID (UUIDv5 per PSP specification), and Atom self-link for the feed URL.

Episode level (item) configuration includes: episode title, episode description and show notes (with HTML support), episode number, season number, episode type (full, trailer, or bonus), episode publication date (RFC 2822 format), enclosure tag with URL, file length in bytes, and MIME type, episode duration in HH:MM:SS format, episode cover art URL (if different from show art), episode GUID, episode explicit flag, episode transcript link (SRT or VTT file URL), and chapter markers using the podcast namespace chapters tag.

The feed manager should support feed validation against Apple Podcasts requirements before publishing, feed preview showing how the podcast will appear in directory listings, automatic feed regeneration when a new episode is published through PodPublisher, manual feed editing for correcting metadata on previously published episodes, feed hosting where Mcaster1Studio can serve the RSS XML file directly via its built in HTTP server or export it for upload to an external web server, multiple show management for podcasters who run more than one podcast, episode scheduling with future publication dates (the episode metadata exists in the feed but is not visible to directories until the publication date), and feed analytics tracking by embedding tracking prefixes in the enclosure URLs for download counting.

### Module 9: M1S-PodTranscribe

Full Name: Mcaster1Studio Podcast Transcriber
Purpose: Speech to text transcription of podcast episodes for accessibility, SEO, and show notes.
Depends On: M1S-PodRecorder or M1S-PodEditor for source audio.

This module transcribes the podcast episode audio into text. It should support real time transcription during recording (live captioning), post-recording transcription for higher accuracy, speaker diarization (identifying and labeling different speakers: "Host:", "Guest 1:", etc.), timestamp markers synced to the audio timeline for easy navigation, confidence scoring on transcribed segments to highlight areas that may need manual review, transcript editing interface with playback sync (click on a word to jump to that point in the audio), export formats including plain text, SRT subtitles (for video podcast platforms), VTT subtitles (WebVTT for web players), Word document, and JSON (structured format with timestamps and speaker labels), show notes generation from the transcript (summarizing key topics and timestamps), chapter marker generation by detecting topic changes in the conversation, and SEO keyword extraction from the transcript for episode descriptions.

The transcript output should be compatible with the PodRSS module for including transcript links in the RSS feed per the podcast namespace transcript tag specification.

### Module 10: M1S-PodSoundboard

Full Name: Mcaster1Studio Podcast Soundboard
Purpose: Instant audio clip triggering for sound effects, stingers, jingles, and music beds.
Depends On: M1S-PodMixer for audio routing (feeds into a mixer channel or dedicated bus).

This module provides a grid of triggerable audio pads for playing sound effects, intro/outro stingers, transition jingles, applause, laugh tracks, music beds, and any other audio clip the podcaster wants to fire during recording or live broadcast. It should support a configurable grid layout (4x4 default, expandable to 8x8), per-pad audio file assignment via drag and drop or file browser, per-pad volume control, per-pad color coding and label for visual identification, trigger modes per pad (one-shot: plays once and stops, loop: repeats until stopped, toggle: starts on first press and stops on second press, hold: plays only while the pad is held down), fade-in and fade-out per pad with configurable duration, pad bank switching so the podcaster can have multiple pages of sound pads (Intro/Outro bank, Sound Effects bank, Music Beds bank, Interview Stingers bank), keyboard shortcut assignment per pad for hands-free triggering, ducking behavior where music bed pads automatically reduce volume when a voice channel is active on the mixer (sidechain ducking), and drag and drop reordering of pads within the grid.

The soundboard routes its audio into a dedicated channel on the PodMixer so the podcaster has full mix control over the soundboard level relative to the voice channels.

### Module 11: M1S-PodShowNotes

Full Name: Mcaster1Studio Podcast Show Notes Editor
Purpose: Rich text editor for creating episode show notes, descriptions, and supplementary content.
Depends On: M1S-PodTranscribe for transcript-based content suggestions.

This module provides a dedicated editor for writing the episode show notes that accompany the podcast in the RSS feed and on the podcast website. It should support rich text editing with headings, bold, italic, links, and lists, timestamp link insertion (clickable timestamps that jump to specific points in the episode), guest bio templates for quickly inserting guest information, resource link management for organizing URLs mentioned during the episode, chapter marker editing that syncs with PodRSS chapter tags, show notes templates that can be saved and reused (standard episode template, interview template, solo episode template), Markdown and HTML export for compatibility with different publishing platforms, word count and reading time estimate, SEO preview showing how the episode will appear in search results with the current title and description, and integration with PodTranscribe to pull key topics and timestamps from the transcript as a starting point for the show notes.

### Module 12: M1S-PodRemote

Full Name: Mcaster1Studio Podcast Remote Guest Module
Purpose: Remote guest connection for recording interviews and multi-host episodes over the network.
Depends On: M1S-PodMixer for audio routing.

This module handles connecting remote guests to the podcast recording session. It should support WebRTC based peer-to-peer audio connections for low latency remote recording, double-ender recording where each participant's audio is recorded locally on their device and synced in post-production for maximum quality regardless of network conditions, VOIP/SIP connectivity for phone call integration, a guest invitation system that generates a unique URL the guest opens in their browser to join the session (no software installation required on the guest side), per-guest audio routing into individual PodMixer channels, per-guest monitoring mix so each guest hears the right balance of other participants, connection quality monitoring showing latency, packet loss, and audio quality per guest, echo cancellation and noise suppression on the remote feed, up to four simultaneous remote guest connections, and a green room / waiting room where guests can test their audio before joining the live recording.

### Module 13: M1S-PodAnalytics

Full Name: Mcaster1Studio Podcast Analytics Dashboard
Purpose: Episode and show performance tracking and analytics.
Depends On: M1S-PodRSS for feed data, M1S-PodPublisher for download tracking.

This module provides analytics for tracking podcast performance. It should support download counts per episode and per show over time, listener geographic distribution (if available from the hosting platform), listening duration and drop-off analysis (where listeners stop the episode), subscriber count tracking across directories, episode comparison charts for identifying which episodes perform best, growth trend visualization (weekly, monthly, quarterly), export of analytics data to CSV or PDF for reporting, and integration with podcast prefix analytics services (Chartable, Podtrac, OP3) by inserting tracking prefixes into the enclosure URLs in the RSS feed.

---

## Module Dependency Map

```
Foundation Layer:
  M1S-PodMixer (audio routing backbone)

Recording Layer (depends on PodMixer):
  M1S-PodPTT (microphone input and voice processing)
  M1S-PodRecorder (multi-track capture)
  M1S-PodSoundboard (audio clip triggering)
  M1S-PodRemote (remote guest audio input)

Processing Layer (depends on Recording):
  M1S-PodFX (effects processing chain)
  M1S-PodEditor (non-destructive editing)

Output Layer (depends on Processing):
  M1S-PodEncode (format encoding and tagging)
  M1S-PodTranscribe (speech to text)

Content Layer (depends on Transcription):
  M1S-PodShowNotes (episode content creation)

Distribution Layer (depends on Encoding):
  M1S-PodRSS (feed generation and management)
  M1S-PodPublisher (file upload and distribution)

Analytics Layer (depends on Distribution):
  M1S-PodAnalytics (performance tracking)
```

## Build Order Recommendation

Phase 1: Foundation
  1. M1S-PodMixer (six channel mixer, the core of the surface)
  2. M1S-PodPTT (enhanced push-to-talk for podcast mic handling)

Phase 2: Recording
  3. M1S-PodRecorder (multi-track recording engine)
  4. M1S-PodSoundboard (sound effects and music bed triggering)

Phase 3: Production
  5. M1S-PodFX (effects rack for voice processing)
  6. M1S-PodEditor (post-production editing)

Phase 4: Encoding and Metadata
  7. M1S-PodEncode (format conversion and ID3 tagging)
  8. M1S-PodTranscribe (speech to text transcription)
  9. M1S-PodShowNotes (episode show notes editor)

Phase 5: Distribution
  10. M1S-PodRSS (RSS feed generation and management)
  11. M1S-PodPublisher (multi-target publishing with ICY 2.2, Icecast2, SFTP, SCP, SSH, HTTP)

Phase 6: Analytics
  12. M1S-PodAnalytics (performance dashboard)

Phase 7: Remote Production
  13. M1S-PodRemote (remote guest connections via WebRTC)

---

## Publishing Protocol Details

### ICY 2.2 Publishing to Mcaster1DNAS

Mcaster1DNAS is our own Digital Network Audio Server platform built on the ICY 2.0/2.2 protocol. The PodPublisher must implement a native ICY 2.2 source client that authenticates with the DNAS server, pushes audio content (encoded podcast episode), updates metadata (episode title, description, show info), and manages mount points for on-demand podcast content delivery. This is not a live stream connection but a content upload and catalog management protocol where the podcast episode is uploaded to the DNAS server and made available for on-demand playback through the DNAS HTTP delivery system.

### Icecast2 Source Publishing

Standard libshout compatible source client connection using the Icecast2 source protocol. This handles authentication with the Icecast2 server, mounting to a specific mount point, and pushing the encoded audio. For podcast use, this could be used for live podcast streaming (recording and streaming simultaneously) or for uploading pre-recorded episodes to an Icecast2 server configured for on-demand content.

### SFTP/SCP/SSH Publishing

Secure file transfer using OpenSSH compatible protocols. The publisher must support SSH key authentication (RSA, ED25519), password authentication, known_hosts verification, configurable port numbers, remote directory creation if the target path does not exist, and file permission setting after upload.

### HTTP/HTTPS REST API Publishing

POST or PUT based file upload using multipart/form-data or binary body. Must support custom headers for API authentication (Bearer token, API key), configurable endpoint URLs, JSON response parsing for upload confirmation, and webhook callback URLs for notifying external systems that a new episode has been published.

---

## Podcast Surface Technical Notes

The Podcast Surface is designed as a complete self-contained production environment. Unlike the DJ Surface which is optimized for real-time on-air performance, the Podcast Surface prioritizes recording quality, post-production flexibility, and end-to-end publishing workflow.

The PodMixer is the routing backbone. All audio in the Podcast Surface flows through the mixer. Microphones connect through PodPTT modules into mixer channels. The soundboard feeds into a mixer channel. Remote guests each occupy a mixer channel. The mixer master output feeds both the PodRecorder and optionally a live stream encoder.

The workflow is linear: configure the mixer (PodMixer), set up mics (PodPTT), record the episode (PodRecorder with PodSoundboard for drops), edit in post (PodEditor with PodFX processing), encode the final file (PodEncode), generate show notes and transcript (PodShowNotes and PodTranscribe), build the RSS entry (PodRSS), and publish to all targets (PodPublisher). PodAnalytics tracks performance over time.

The surface tab in the main Mcaster1Studio tab bar shows "Podcast" with its own independent ON AIR / OFF AIR state. When the Podcast Surface is ON AIR, it means the live podcast is being streamed. When it is OFF AIR, the podcaster is in recording, editing, or publishing mode without a live stream.

All Podcast Surface modules share the same visual design language as the rest of Mcaster1Studio: dark themed UI, module windows with consistent title bars (minimize, maximize, detach, info, close controls), and the same status bar integration showing recording state, elapsed time, and encoder status.