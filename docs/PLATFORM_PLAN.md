# The core functions: a plan

What Tessera builds next on the ESP32-P4 boards, and in what order: a hardware layer the shared code talks to instead
of the chip, memory and live cameras with limits a user can read, and the ground for a doorbell you can talk back to.
Written 2026-10-25 after the P4 memory work (CHANGELOG, heap_watch, psram_task) and a review of the seams the code has
today; docs/PICTURE_ARCHITECTURE.md says how the picture path is built, this page says what changes and why. The rules
that already hold stay (AGENTS.md: behaviour lives once, a board file holds hardware and sizes only, no capability rules
by hand).

## 1. Where we stand

A few thousand screens run Tessera, a few hundred of them on a P4. The S3 boards keep their refreshed stills and stay
out of this page. Everything with video is the P4's: live camera tiles, the camera full screen with its sound, the
screen's own camera as video for Home Assistant, and a doorbell that rings on the glass. Users now ask for five live
tiles on one screen, and the doorbell should one day hear them back and open the door.

What the review found (2026-10-25, branch dev):

- **The picture path has its seam.** `picture_engine.h` (decode, blit, client) and `glass.h` (frame buffer, rooms,
  one-writer lock) are the only places the shared code meets the chip; `p4_build.py` picks the engine and the linker
  wraps, and S3 boards and the host get inline stubs for free. No `#if` on a chip name exists in `smart_display`.
- **Three chip facts leak out of the engine:** `glass::PPA_BURST_BYTES`, the PPA's sixteenths in
  `live_view::scaled()`, and the picture sizes of `live_view.h` (`MAX_PICTURE`, `TILE_PICTURE`) that come from P4
  measurements.
- **The Tab5 is a second P4.** `hardware/m5stack-tab5.yaml` does not include `esp32p4-c6.yaml`: it carries its own
  esp32 and hosted block and misses what the other P4 boards got this week (`CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM`,
  the 20 s watchdog). The Tab5 tester's restarts every few minutes are consistent with that.
- **Audio has no owner.** The camera's sound and Home Assistant's media both `start()`, `stop()` and
  `set_audio_stream_info()` on `ts_media_speaker` with nothing between them; the voice plugin, the tap sound and the
  doorbell chime share the announcement input through the mixer alone; the microphone is shared by listener count.
  Nothing says who may speak or listen when a doorbell is open.
- **Plugins cannot ask what a board can.** The API (0.18) exposes `ui::pictures()` and nothing else; `ui::live()`
  degrades to stills without a word. A board's features reach plugins only as build-time ids
  (`plugin_manifest.py`).
- **Limits are scattered.** The firmware allows seven streams a screen (`live_view::MAX_STREAMS`), the add-on eight of
  all screens together (`live_stream.MAX_STREAMS`), the editor nothing; none of the three knows the others, and a
  refused stream leaves a still without saying why.
- **The network is the ceiling, not the memory.** Espressif measures 30 Mbit/s TCP into a P4 over the C6's SDIO link
  in the open air (esp-hosted-mcu, docs/design/performance.md); a 1280 x 720 picture is about 80 KB at the add-on's
  quality, a full view at 13 pictures a second about 1 MB/s. Five tiles fit the bandwidth, but esp_hosted's own SDIO
  reader drops blocks under sustained inbound load (espressif/esp-hosted-mcu #197, open), which the 20 s watchdog now
  survives as a failed call. The add-on already paces every stream by back-pressure (the newest frame once the socket
  drained), so a slow screen gets fewer pictures, never older ones.
- **Memory is a policy, not a leak.** ESPHome builds the P4 with `CONFIG_SPIRAM_USE_CAPS_ALLOC`, so every plain
  allocation takes internal RAM and each consumer must be moved to PSRAM by hand (`roomy.h`, `psram_task.h`, the
  esp_hosted and lwIP options). Espressif's own P4 default is `SPIRAM_USE_MALLOC` with an internal threshold. That is
  the structural fix and an ESPHome-wide one.

## 2. The order

1. **The hardware layer** (section 3): one P4 core file, the chip facts back in the engine, a capability record the
   firmware, the add-on, the editor and the plugins all read, and an audio owner. Future-proof work, mostly moves.
2. **Memory and cameras with limits** (section 4): a cap on live tiles that three places share, a memory governor
   that stops a stream before the heap runs out, the soak that measures the SDIO link, and then the allocation policy.
3. **The doorbell's talk-back** (section 5): prepared, not built. The audio owner from step 1 and an uplink the add-on
   bridges are the whole preparation.

Each step lands on dev in pieces its testers can run, with a CHANGELOG line where a user would notice.

## 3. The hardware layer

### 3.1 One P4 core file

`packages/hardware/esp32p4-core.yaml` holds what every P4 shares whatever its radio wiring and panel: the chip, its
frequency and revision switch, PSRAM mode and speed, `execute_from_psram`, the watchdog, and the whole `sdkconfig_options`
block (esp_hosted mempool, PSRAM task stacks, `MEMPOOL_PREFER_SPIRAM`, thread-local pointers). `esp32p4-c6.yaml` and
`m5stack-tab5.yaml` include it and keep only their pins, SDIO frequency and LDO. A Python test asserts that every board
whose variant is esp32p4 reaches the core file, so the next P4 cannot miss it.

Done when: the Tab5 builds from the core file with no sdkconfig of its own, and `tests/test_live_view.py` (or a new
`test_boards.py`) fails on a P4 board file without it.

### 3.2 The chip facts back behind the seam

- `picture_engine::limits()` returns what today sits in `live_view.h` and `glass.h`: the burst bytes, the largest
  picture and tile picture, the scale steps (sixteenths), the largest side the decoder takes. The software engine
  returns zeros, and `live_view` and `glass` read the record instead of constants.
- `glass.h` loses `PPA_BURST_BYTES`; `glass_dsi.cpp` asks the engine.
- `PICTURE_ARCHITECTURE.md` section 2.4 (the table of every hardware fact and where it is read) is brought up to date
  (the panel wrap lives in `glass_dsi.cpp` now) and gains the rule: a chip fact is read from `picture_engine::limits()`
  or a wrap, never from a header the shared code includes.

Done when: `grep -rn "PPA\|sixteenth\|MIPI\|JPEG" components/smart_display/*.h` outside `picture_engine*.h`,
`ppa_guard.h`, `picture_jpeg.h` and `glass_dsi.cpp` finds comments only, and a test keeps it so (the way
`test_live_view.py` keeps `DIAGNOSTIC_WRAPS` to what `abort_record.cpp` defines).

### 3.3 One capability record

Today the hello sends "live" and "doorbell" when `live_view::available()` is true, `boards.json` says `camera` per
board, and plugins guess. One record replaces the three:

```
struct Caps {           // hw.h, read at boot, constant after
  bool live_video;      // picture_engine::JPEG && glass::direct()
  bool speaker, microphone, echo_cancel, camera;
  uint8_t live_tiles;   // the most live tiles this board runs at once (section 4.1)
  uint32_t psram_bytes, internal_bytes;
};
const Caps &caps();
```

- The firmware fills it from the engine, the glass and the audio and camera ids the board file binds (the
  `BOOT_AUDIO` block already knows them).
- The hello lists the features it implies ("live", "doorbell", "sound", "talk" later) and the numbers (`live_tiles`),
  so the add-on and the editor read the screen's own word, not `boards.json`'s guess. `boards.json` stays for screens
  that have not said hello yet.
- The plugin API (0.19) gets `tessera::hw::caps()`, read-only. `ui::live()` keeps degrading to stills, but a plugin can
  now say why on its own card.

Done when: the add-on's `live_pace`, the doorbell's feature check and the editor's live-tile count all read the hello,
and `FIRMWARE_API.md` documents `hw::caps()` with the rule that a plugin never infers hardware from a board name.

### 3.4 An audio owner

One module, `audio_owner.h`, pure and host-tested, with the ESPHome speakers and microphone bound at boot as
`live_view::bind_speaker` is today:

```
enum class Role { DOORBELL, CAMERA_SOUND, VOICE, MEDIA, SOUND };   // highest first
bool claim(Role, Side side /* OUT, IN, BOTH */, std::function<void()> lost);
void release(Role, Side);
Role holder(Side);   // what plays or listens now
```

- A higher role takes the side and calls the lower holder's `lost`; a lower role is refused and may try again.
- The camera's sound claims OUT when the full view opens with sound and releases at its end; Home Assistant's media
  claims OUT through the media player's start; the voice plugin claims IN for the wake word and BOTH for a session;
  the doorbell claims BOTH for a ring with talk-back (section 5).
- The half-duplex interval and the mixer stay as they are: the owner decides who may start, the mixer still mixes
  what two holders of different inputs play (an announcement over media).
- `hw::caps()` tells a plugin the sides exist; `tessera::audio::holder()` tells it whether to speak now. The voice
  plugin stops its wake word while the doorbell holds IN, which is what Max asked for.

Done when: opening a camera with sound while music plays, and a doorbell ring while the wake word listens, each leave
one holder per side and the loser's `lost` runs, on the host test and on a D1001.

### 3.5 Allocation as a seam

`roomy.h` and `psram_task.h` are the memory layer of the shared code. The rule, written in `PICTURE_ARCHITECTURE.md`
section 8 and kept by a grep test: feature code (`live_view`, `picture_fetch`, modules, plugin host) calls `roomy` and
`psram_task`, never `heap_caps_*` or `xTaskCreate*` directly; only the two headers and the engines may. The test lists
the files allowed to say `heap_caps` so a new one fails the check until it is reviewed.

### 3.6 The screen's own camera

Screen camera's video encoder registers its own PPA client and hard-codes the burst length. It should use
`picture_engine::client()` and a `convert()` operation the engine gains for YUV input, so the one-2D-DMA-user rule and
the burst fix hold for it too. Low urgency: it works and the wraps already catch its calls. Noted so the plugin API
grows the right way.

### 3.7 Declare below, resolve above

More boards come every few months, so the layering is the product. The rule, from the bottom up, each layer knowing
only the one under it:

1. **The board file declares parts and pins.** `packages/boards/<board>.yaml` says "the microphone is an ES7210 on
   these I2S pins, the speaker an ES8311 on these, the camera an SC202CS on this I2C bus, the panel this size at this
   DPI" and nothing else: substitutions and includes, no logic, no ids of its own (AGENTS.md: a board file holds
   hardware and sizes only). Today the audio transports are chosen by hand (`TAB5_AUDIO_TRANSPORT`,
   `P4_AUDIO_TRANSPORT`, `es7210-aec.yaml`, `guition-p4-audio.yaml`); the board should name the part and the part file
   should know its transport.
2. **A part file resolves a part to ESPHome components.** `packages/hardware/parts/<part>.yaml`, one per codec, sensor
   or panel (`es7210.yaml`, `es8311.yaml`, `sc202cs.yaml`, …), turns the declaration into the components and binds them
   to the fixed ids the layer above expects: `ts_microphone`, `ts_speaker_out`, `ts_camera`, and a few facts as
   substitutions (`AUDIO_MIC_AEC: true`, `AUDIO_SAMPLE_RATE`). A part that brings echo cancellation declares it here.
3. **The feature layer builds on the ids alone.** `features/audio.yaml`, `audio-player.yaml`, `camera.yaml` never see a
   pin or a chip: they wire `ts_microphone` into the mixer, the resampler, the owner (3.4) and the plugins' ids. A board
   without a part leaves an id unbound, and the feature file is skipped by the board's includes, as `features/audio.yaml`
   already is.
4. **The firmware resolves what exists at boot** into `hw::caps()` (3.3) and the top layer says "the microphone of this
   board": `audio_owner::claim(Role::VOICE, Side::IN)`, `picture_engine::decode`, `glass::reserve`. No module and no
   plugin knows a chip name, a pin or a transport.

What this needs beyond 3.1 to 3.4: a `parts/` directory with the codecs and sensors the boards use today (five audio
parts, three camera sensors, the panel drivers), each board file rewritten to declarations only, and a Python test
that reads every board file and fails on anything but substitutions and includes. That test is a ratchet: it lists,
per top-level key a board file may still hold, the boards that hold it (`display: [tab5, wavesharep4, ...]`), fails on a
key or a board that is not listed, prints what is left when it passes, and a key leaves the list once its boards are
empty. Audio and the camera become parts first, with the Tab5 and the Waveshare set as the proof that a board builds to
the same sdkconfig and components from declarations; the panels and touch come last, because their files carry each
board's LVGL settings today and move only once those settings have a part of their own. `PROFILES.md` describes the
layering; it gains the part layer and the rule "declare below, resolve above". `ADDING_A_BOARD.md` then says: name the
parts, give the pins, done.

## 4. Memory and cameras with limits

### 4.1 A cap on live tiles that three places share

Four live tiles a screen, plus the full view (which halts the tiles anyway), on every P4 board; a fifth live tile on a
page shows the camera's still with a small "live paused" badge and a line in its settings ("This screen plays 4 cameras
live at once; the others show a picture every 15 seconds"). The number lives in one place, the firmware's
`hw::caps().live_tiles`, and reaches:

- the firmware's `live_tiles_round`, which starts the first four by tile order and leaves the rest at
  `DEFAULT_PACE`;
- the add-on, which reads it from the hello and refuses a fifth stream of that screen with the 503 it already has for
  the global eight, and sets the editor's text;
- the editor, which shows the count on the page and the badge on the fifth tile as it is placed.

Why four: the soak numbers (two tiles plus the video held 175 KB internal RAM free with no failed allocation) leave room
for two more; the SDIO link carries it; and a user who reads "4" trusts the screen more than one who finds out which
fifth tile silently stopped. `MAX_STREAMS` stays seven for a page of four tiles, the full view and plugin places; it is
a ceiling, not the cap.

### 4.2 The source scales, not the screen

The add-on knows every room it serves. Two rules it gains, both in `live_pictures`/`live_stream`:

- A tile room smaller than a quarter of the panel gets its pictures at most 8 a second, the full view at the camera's
  pace. The screen already shows what it gets; the add-on simply makes fewer.
- A screen's streams together stay under a byte budget per second it states in its hello (the P4's: 1.5 MB/s, half the
  measured TCP ceiling). Over it, the add-on lowers tile pictures a second first, then their JPEG quality (QSCALE 5 to
  8), never the full view's.

Done when: five tiles on a D1001 stream within the budget and `heap_watch` logs no failed allocation over an hour.

### 4.3 A memory governor

`heap_watch` sees the internal low-water mark every 250 ms. Instead of logging alone, it offers one action: when the
lowest since boot falls under 48 KB, `live_tiles_round` stops the newest live tile and marks it paused with the badge;
under 32 KB it stops the next; the full view is never stopped. A tile stopped this way starts again after five minutes
with room. The thresholds are constants in `heap_watch.h` with the measurements beside them, and the CHANGELOG says a
screen now pauses a camera before it would restart.

### 4.4 The soak that measures the link

Before the release candidate goes out, on a D1001 with four live tiles, the screen's own video and the voice assistant
in use, 24 to 48 hours, counted per hour: esp_hosted drops ("task still writing Rx data to queue"), RPC timeouts,
watchdog and abort restarts, the task count and both heaps (heap_watch's five-minute line). Then the same with
`CONFIG_ESP_HOSTED_USE_MEMPOOL` back on beside `MEMPOOL_PREFER_SPIRAM`, so the pool sits in PSRAM and every packet
stops going through `heap_caps_aligned_alloc`. The configuration with the lower drop count is the release's. The three
failure paths of `psram_task` are forced once in a test build (a start without PSRAM room, a download that overflows
its stack, a stream that ends while the main loop is in a long lay-out), and the lifecycle gets a host test.

### 4.5 The allocation policy, after the release

A dev cycle of its own: build the P4 with `CONFIG_SPIRAM_USE_MALLOC`, `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` at 16 KB
and `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL` at 64 KB, so strings, vectors, plugin documents and TLS land in PSRAM
without `roomy` asking. ESPHome hard-codes `SPIRAM_USE_CAPS_ALLOC` in its psram component, so this needs either an
upstream option or a sdkconfig override that the build verifies; the soak of 4.4 is the measure, LVGL's draw times the
risk. If it holds, `roomy` becomes a thin name for "large and read by the screen alone" rather than the only road to
PSRAM.

## 5. The doorbell's talk-back, prepared

Not built now: Home Assistant is expected to bring a two-way audio path of its own within months, and the community
VoIP stack (a SIP endpoint inside Home Assistant) already bridges door stations. What the firmware prepares does not
depend on which one wins:

- **The owner.** Section 3.4: a ring claims BOTH, the wake word steps back, the camera's sound keeps playing. That is
  the "always hear the other side" rule.
- **Push to talk.** The doorbell view gets a hold-to-talk key. While held, the microphone's 16 kHz PCM goes up the
  camera port as one more stream (a POST of `audio/x-s16le;rate=16000` on `/camera/<token>/talk`), the speaker ducks
  the camera's sound (the mixer's `apply_ducking`), and echo cancellation is not needed. Released, the uplink closes.
- **The add-on bridges.** The screen never speaks to a doorbell. The add-on takes the PCM and hands it to the camera's
  back channel through go2rtc (Reolink takes G.711 there) or to Home Assistant's integration when it exists; the
  transcoding is ffmpeg's, which the add-on already has. Reolink first, because testers have one.
- **Unlock** is a Home Assistant action the add-on fires from the view's key, as the doorbell events already are.

Done when (later): a tester holds the key on a D1001 and is heard at a Reolink doorbell, with the doorbell's sound on
the glass before, during (ducked) and after.

## 6. What stays out

- Replacing ESPHome. Its board files, build server, override YAML, OTA and Home Assistant integration are why boards
  are cheap to add; the layer above (sections 3 and 4) is ours and stays ours.
- Ethernet or another radio as a way around the SDIO link. Boards will not have it; the link's budget is a rule the
  add-on keeps.
- H.264 on the screen. The P4 encodes, it does not decode; the add-on's JPEGs stay the format.
- A fifth live tile "when there is room". The cap is the feature.

## 7. Evidence to keep

Every step lands with its numbers in the commit message, as the P4 memory work did: the board, the date, internal RAM
free and lowest, pictures a second, drops an hour. Docs hold the rule that resulted, not the log (AGENTS.md).
