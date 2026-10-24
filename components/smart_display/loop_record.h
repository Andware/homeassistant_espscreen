// The main loop at the task watchdog. ESPHome's crash record keeps the fault location of the task that was running on
// the crashed core, and when the task watchdog restarts a screen because the main loop (loopTask) stopped feeding it,
// that task is nearly always the idle one: the record then says `esp_cpu_wait_for_intr` and nothing of what the main
// loop waited on. Two P4 screens of a dev tester restarted that way on 2026-10-23 and 24 (TR-4EFBDE, TR-FNFW9J), both
// with the esp_hosted SDIO wedge in the log (espressif/esp-hosted-mcu #197) and nothing to prove the chain.
//
// So the watchdog's own hook (`esp_task_wdt_isr_user_handler`, which ESP-IDF calls in the watchdog's interrupt before
// it aborts) takes the main loop's stack as it stands, blocked or running, and keeps every word of it that points at
// code, newest first, in memory the reset leaves alone. The next boot logs them as `LOOP0: 0x...` lines after
// ESPHome's own crash record; the troubleshooting report keeps them with the crash (reports.py CRASH_LINE) and decodes
// them with the ELF, and `addr2line -pfiaC -e firmware.elf <addresses>` does the same by hand. On a RISC-V chip the
// first two are the saved frame's MEPC and RA, the rest a scan of the stack: return addresses of the callers with,
// between them, whatever else on the stack happens to look like code, which addr2line names as `??` and the report
// drops. Read the chain for the names that make sense together.
#pragma once
#include <cstdint>

namespace loop_record {

constexpr int MOST = 24;  // addresses kept, enough for the chain from the wait to the component that waited

// The words from `from` (the top of the stack, the newest) up to `end` that `is_code` takes for code, at most `most`.
template <typename IsCode>
inline int scan(const uint32_t *from, const uint32_t *end, uint32_t *out, int most, IsCode is_code) {
  int n = 0;
  for (const uint32_t *p = from; p < end && n < most; ++p)
    if (is_code(*p)) out[n++] = *p;
  return n;
}

// At boot: logs the record the last task watchdog left, once, and forgets it.
void report();

}  // namespace loop_record
