// The Wi-Fi co-processor of an ESP32-P4 screen: an ESP32-C6 on SDIO through esp_hosted (hardware/esp32p4-c6.yaml,
// hardware/m5stack-tab5.yaml).
//
// Its reset. esp_hosted resets the C6 at every start of the P4: the reset line at the level the C6 runs at, 10 ms at
// the other, back, and 1.5 s for the C6 to boot (transport_gpio_reset in its sdio_drv.c). On a Guition JC8012P4A1 V3
// whose C6 runs ESP-Hosted 2.3.2 those 10 ms did not reset it: after a software restart (an update over the air, a
// crash, the watchdog) the C6 kept its old SDIO session, every read of the new one failed ("sdmmc_host_wait_for_event
// returned 0x107", "sdio_read_task: Failed to read data - -1 838860 838860", the C6's byte count from before the
// restart), esp_hosted restarted the P4 after 13 s, and so on until the power went off. The bootloader's rollback to
// the previous firmware looped the same way, safe mode included. A cold start was fine: the C6 then starts by itself
// (the bench board, 2026-10-26; another V3 of the same chip revision came back from five watchdog restarts, so it is
// the C6's firmware or the board, not every V3). So the P4 holds the C6 in reset from the first moment of its start,
// before ESPHome and its safe mode, until esp_hosted sets the line itself: a reset as long as the P4's own boot,
// whatever restarted it. The pin and its levels are esp_hosted's (its sdkconfig, from the board's esp32_hosted block),
// never a number here. Measured on the bench board: 2 of 2 software restarts hung without the hold, 3 of 3 (two updates
// and an abort) came back with it, Wi-Fi 19 s after each.
//
// Its firmware. The C6 keeps its own, which flashing the P4 never changes. firmware() asks it once, with Wi-Fi up, logs
// it and gives it to the "Wi-Fi co-processor" sensor (hardware/esp32p4-core.yaml), which the troubleshooting report
// reads, so the reports tell which C6 firmwares the screens out there run.
#pragma once

#include <string>

namespace wifi_chip {

// What the co-processor runs, "ESP-Hosted 2.3.2 (host 2.12.12)"; "unknown" while Wi-Fi is not up yet, and on a screen
// without one. The first call with Wi-Fi connected asks the C6 (one RPC over the link); every later one gives that.
std::string firmware();

}  // namespace wifi_chip
