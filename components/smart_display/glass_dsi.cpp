// The glass of an ESP32-P4's MIPI-DSI panel (glass.h says what and why; glass_core.cpp has what every panel kind shares):
// the panel ESPHome's display made, picked up as ESP-IDF makes it, its frame buffer, the panel's underrun counter and the
// PPA bursts of every client.
#include "glass.h"
#include "picture_engine.h"
#if defined(GLASS_DSI) && defined(SCREEN_PICTURES)
#include <driver/ppa.h>
#include "lvgl.h"  // LV_COLOR_16_SWAP, LVGL's byte order for this display
#include <esp_lcd_mipi_dsi.h>
// The DSI bridge's own registers, for its underrun flag (misses below); ESP-IDF keeps the chip's registers here.
#if __has_include(<soc/mipi_dsi_bridge_struct.h>)
#include <soc/mipi_dsi_bridge_struct.h>
#define GLASS_COUNTS_MISSES 1
#endif

namespace glass {

// ---- The panel ESPHome's display made (the wrap of esp_lcd_new_panel_dpi at the end of this file) ----
static esp_lcd_panel_handle_t panel_handle = nullptr;
static Panel panel_facts;
static void *buffer = nullptr;

static void panel_made(esp_lcd_panel_handle_t made, const esp_lcd_dpi_panel_config_t *config) {
  panel_handle = made;
  panel_facts.swapped = LV_COLOR_16_SWAP;
  panel_facts.w = config->video_timing.h_size;
  panel_facts.h = config->video_timing.v_size;
  // ESPHome names the frame buffer's format in either field, by ESP-IDF version.
  panel_facts.rgb565 = config->in_color_format == LCD_COLOR_FMT_RGB565 ||
                       (config->in_color_format == 0 && config->pixel_format == LCD_COLOR_PIXEL_FORMAT_RGB565);
}
Panel panel() { return panel_facts; }
bool direct() { return panel_handle != nullptr && panel_facts.rgb565 && bound(); }
void *frame_buffer() {
  if (!buffer && panel_handle && esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 1, &buffer) != ESP_OK) buffer = nullptr;
  return buffer;
}

// ---- The panel's share of the memory bus ----
// ESP-IDF only logs an underrun from its interrupt, on the UART and for every frame. Here the bridge's underrun flag is
// read every tick instead (its interrupt is turned off, so the flag stays up until it is read) and counted, so a
// tester's log says whether a change helped (the Guition 10.1 P4, pixel clock 70 MHz, saw the flashes; the D1001 at
// 80 MHz counts none).
static uint32_t panel_misses = 0;
#ifdef GLASS_COUNTS_MISSES
static bool counting_misses = false;
static void count_misses() {
  if (!counting_misses) {
    MIPI_DSI_BRIDGE.int_ena.underrun_int_ena = 0;
    MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1;
    counting_misses = true;
    return;
  }
  if (MIPI_DSI_BRIDGE.int_raw.underrun_int_raw) {
    MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1;
    ++panel_misses;
  }
}
#else
static void count_misses() {}
#endif
void tick() {
  if (direct()) count_misses();
}
uint32_t misses() { return panel_misses; }

}  // namespace glass

// ESP-IDF makes the DPI panel for ESPHome's display (mipi_dsi, or the repo's mipi_dsi_v3 copy): its handle and its
// size are picked up here as they are made (-Wl,--wrap=esp_lcd_new_panel_dpi, smart_display/__init__.py on a P4).
extern "C" esp_err_t __real_esp_lcd_new_panel_dpi(esp_lcd_dsi_bus_handle_t bus, const esp_lcd_dpi_panel_config_t *config,
                                                   esp_lcd_panel_handle_t *made);
extern "C" esp_err_t __wrap_esp_lcd_new_panel_dpi(esp_lcd_dsi_bus_handle_t bus, const esp_lcd_dpi_panel_config_t *config,
                                                   esp_lcd_panel_handle_t *made) {
  const esp_err_t err = __real_esp_lcd_new_panel_dpi(bus, config, made);
  if (err == ESP_OK && made && config) glass::panel_made(*made, config);
  return err;
}
// Every PPA client on this screen, ESPHome's for LVGL's turned flushes included, moves in the engine's burst, so
// the panel's DMA gets its turn on the bus ("The panel's share of the memory bus", glass.h). The streams' own clients
// ask for it already; a client that asks for less keeps its own.
static_assert(picture_engine::limits().burst_bytes == 64, "the burst below is the engine's");
extern "C" esp_err_t __real_ppa_register_client(const ppa_client_config_t *config, ppa_client_handle_t *ret_client);
extern "C" esp_err_t __wrap_ppa_register_client(const ppa_client_config_t *config, ppa_client_handle_t *ret_client) {
  if (!config) return __real_ppa_register_client(config, ret_client);
  ppa_client_config_t own = *config;
  // A zeroed config means ESP-IDF's default, the longest burst.
  if (own.data_burst_length == 0 || own.data_burst_length > PPA_DATA_BURST_LENGTH_64) own.data_burst_length = PPA_DATA_BURST_LENGTH_64;
  return __real_ppa_register_client(&own, ret_client);
}
#endif
