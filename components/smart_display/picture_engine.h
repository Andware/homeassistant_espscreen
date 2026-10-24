#pragma once
// The picture engine: decode, scale, turn (docs/PICTURE_ARCHITECTURE.md, section 3.2). What turns a picture's bytes into
// pixels and pixels into other pixels, and which formats this screen's decoder takes. Two engines, chosen when the
// firmware is built, as the firmware chooses its modules: on an ESP32-P4 with a MIPI-DSI panel (USE_LIVE_VIEW and
// pictures) the hardware JPEG decoder and the PPA, with ppa_guard between (picture_engine_p4.cpp); everywhere else the
// software engine, which is the BMP decoder row by row (picture_fetch::Decoder) and no blit at all, since LVGL draws
// every still there: the inline stubs below, which cost no flash.
//
// The engine knows the decoder, the PPA, the guard and the decoder's memory; it knows nothing of streams, sockets,
// owners or LVGL objects. The glass (glass.h) is held around every decode and every blit: by the engine itself for a
// still it decodes into a buffer of its own (decode), by the caller for a stream's decode into the stream's own buffer
// (decode_into) and for a blit, so the stream's task keeps its stop check under the same lock it had.
//
// The 2D-DMA's own facts are the engine's and nobody else's: its units of 64 pixels for 16-bit output and 32 for 24- and
// 32-bit, its macro block of 16 rows on every P4 build ESPHome makes, its FIFO of 1536 bits and the turned block with a
// small leftover it never finishes (DIG-734), as ppa_guard.h holds them; every PPA call of the firmware, the streams',
// LVGL's and a plugin's, passes the guard through the linker's wrap (ppa_guard.cpp). The clockwise turn LVGL counts
// becomes the PPA's counter-clockwise angle in one place, ppa_angle, which tests/test_picture_engine.cpp checks on a PC.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
// USE_LIVE_VIEW comes with ESPHome's defines (smart_display/__init__.py); a PC test has none and builds without it.
#if __has_include("esphome/core/defines.h")
#include "esphome/core/defines.h"
#endif

namespace picture_engine {

// A decoded still: `stride` pixels a row (the decoder writes whole MCUs, picture_jpeg.h), its buffer `bytes` long.
struct Still {
  int w = 0, h = 0, stride = 0;
  size_t bytes = 0;
};
// One copy: the block `bw` x `bh` at bx, by of the picture `in` (`in_w` pixels a row, `in_rows` rows, RGB565) to x, y
// of the picture `out` (`out_w` x `out_h`, `out_size` bytes), scaled by `sixteenths` / 16 and turned `turn` degrees
// clockwise, as LVGL counts its rotation.
struct Block {
  const void *in = nullptr;
  int in_w = 0, in_rows = 0, bx = 0, by = 0, bw = 0, bh = 0;
  void *out = nullptr;
  size_t out_size = 0;
  int out_w = 0, out_h = 0, x = 0, y = 0;
  int sixteenths = 16;
  int turn = 0;
};
// A client of the PPA, one per stream; nullptr for none.
using Client = void *;

// The PPA's angle for a turn as LVGL counts it: the PPA counts counter-clockwise (0, 90, 180, 270 as 0 to 3, the order
// of ppa_srm_rotation_angle_t and of ppa_guard::Angle), so LVGL's 90 is the PPA's 270 and the other way round. Written
// here once; the camera plugin's copy of its driver keeps its own (ppa_rotation_for_clockwise), as a copy must.
inline int ppa_angle(int turn) { return turn == 90 ? 3 : turn == 180 ? 2 : turn == 270 ? 1 : 0; }

#if defined(USE_LIVE_VIEW) && defined(SCREEN_PICTURES)
// This screen's decoder takes JPEG: its questions name it first (runtime_tiles::picture_ask), the download gathers a
// JPEG answer whole (picture_fetch::Gather) and the streams are JPEG (live_view.h).
constexpr bool JPEG = true;
// The formats the decoder takes whole, not row by row as it streams: named first in every question.
const std::vector<std::string> &whole_formats();
// The decoder made, on the main loop; true while it exists. The streams start it as they open.
bool start();
// Stills may be decoded (prepare_stills before): the decoder made, and LVGL holding the glass while it refreshes
// (glass::watch), so a decode in a download's task and LVGL's copy to the panel never run on the 2D-DMA at once.
bool ready();
// A buffer the decoder may read from (`got` what it is, rounded up), and one it may write to. free() them.
uint8_t *input(size_t bytes, size_t *got);
uint8_t *output(size_t bytes, size_t *got);
// A JPEG's size, and the decoded picture's stride and bytes (whole MCUs). False for a JPEG the decoder cannot read.
bool info(const uint8_t *data, size_t length, Still &out);
// A whole JPEG decoded into `into` (`into_size` bytes, at least `out.bytes` of info): RGB565 in LVGL's order. The caller
// holds the glass.
bool decode_into(const uint8_t *data, size_t length, uint8_t *into, size_t into_size);
// A whole JPEG decoded into a buffer of its own (free() it), the glass held meanwhile; nullptr before ready() or for a
// JPEG the decoder cannot read. From a download's task.
uint8_t *decode(const uint8_t *data, size_t length, Still &out);
// A PPA client with the glass's bursts (glass.h), and its end.
Client client();
void drop(Client client);
// The copy, through the guard: 0 when it is done, else ESP-IDF's error. The caller holds the glass.
int blit(Client client, const Block &block);
#else
constexpr bool JPEG = false;
inline const std::vector<std::string> &whole_formats() {
  static const std::vector<std::string> none;
  return none;
}
inline bool start() { return false; }
inline bool ready() { return false; }
inline uint8_t *input(size_t, size_t *got) {
  if (got) *got = 0;
  return nullptr;
}
inline uint8_t *output(size_t, size_t *got) {
  if (got) *got = 0;
  return nullptr;
}
inline bool info(const uint8_t *, size_t, Still &) { return false; }
inline bool decode_into(const uint8_t *, size_t, uint8_t *, size_t) { return false; }
inline uint8_t *decode(const uint8_t *, size_t, Still &) { return nullptr; }
inline Client client() { return nullptr; }
inline void drop(Client) {}
inline int blit(Client, const Block &) { return -1; }
#endif

}  // namespace picture_engine
