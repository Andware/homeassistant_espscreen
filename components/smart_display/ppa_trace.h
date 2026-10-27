// The last calls to the P4's PPA, kept across a restart. A copy the engine never finishes (ppa_guard.h: the 2D-DMA's
// DIG-734, espressif/esp-idf #19023 and #19096) leaves no error and no log: the task that asked waits for good, the
// main loop behind it when LVGL held the glass, and the task watchdog restarts the screen with a record that names only
// the wait (loop_record.h: ppa_do_operation). A reTerminal D1001 restarted that way while its top bar was held
// (2026-10-27), and only a trace of the calls themselves named the copy that never came back: the hold line's 336x6,
// turned a quarter.
//
// So every call through the guard's wrap (ppa_guard.cpp) is noted in a ring of the last few, in memory a reset leaves
// alone: its block, angle, where it lands, the pieces the guard cut it into, the task that asked and whether it came
// back. The next boot takes the ring over and logs it with ESPHome's config dump, as loop_record does, so a log
// subscriber and the troubleshooting report see it next to the main loop's record: a call that never came back is the
// one the screen stopped on.
#pragma once

namespace ppa_trace {

// At boot: takes over the calls the last boot left, if it left any, and starts the ring again.
void take();
// With the config dump: logs the calls taken at boot, the newest last.
void report();

}  // namespace ppa_trace
