// The Wi-Fi co-processor's reset and firmware (wifi_chip.h says why).
#include "wifi_chip.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32_HOSTED
#include <sdkconfig.h>
#endif

#if defined(USE_ESP32_HOSTED) && defined(CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE)
#include <cinttypes>
#include <cstdio>

#include <driver/gpio.h>
#include <esp_hosted.h>
#include <esp_hosted_host_fw_ver.h>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/log.h"

namespace wifi_chip {

namespace {

constexpr const char *TAG = "wifi_chip";
constexpr gpio_num_t RESET_PIN = static_cast<gpio_num_t>(CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE);
// The level that holds the C6 in reset: the other one than it runs at, which esp_hosted calls active.
#ifdef CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW
constexpr uint32_t HELD = 1;
#else
constexpr uint32_t HELD = 0;
#endif

// Before app_main, so before ESPHome's setup and its safe mode: the pin freed of whatever its reset left it as, then
// at the level that holds the C6, until esp_hosted's own reset drives it.
__attribute__((constructor)) void hold() {
  gpio_reset_pin(RESET_PIN);
  gpio_set_level(RESET_PIN, HELD);
  gpio_set_direction(RESET_PIN, GPIO_MODE_OUTPUT);
}

std::string told;

}  // namespace

std::string firmware() {
  if (!told.empty())
    return told;
  auto *wifi = esphome::wifi::global_wifi_component;
  if (wifi == nullptr || !wifi->is_connected())
    return "unknown";
  ESP_LOGD(TAG, "Wi-Fi co-processor held in reset until the host was ready (GPIO%d)", (int) RESET_PIN);
  esp_hosted_coprocessor_fwver_t ver{};
  char text[64];
  if (esp_hosted_get_coprocessor_fwversion(&ver) == ESP_OK) {
    snprintf(text, sizeof(text), "ESP-Hosted %" PRIu32 ".%" PRIu32 ".%" PRIu32 " (host %d.%d.%d)", ver.major1,
             ver.minor1, ver.patch1, ESP_HOSTED_VERSION_MAJOR_1, ESP_HOSTED_VERSION_MINOR_1,
             ESP_HOSTED_VERSION_PATCH_1);
  } else {
    snprintf(text, sizeof(text), "ESP-Hosted unknown (host %d.%d.%d)", ESP_HOSTED_VERSION_MAJOR_1,
             ESP_HOSTED_VERSION_MINOR_1, ESP_HOSTED_VERSION_PATCH_1);
  }
  told = text;
  ESP_LOGI(TAG, "Wi-Fi co-processor: %s", text);
  return told;
}

}  // namespace wifi_chip

#else

namespace wifi_chip {

std::string firmware() { return "unknown"; }

}  // namespace wifi_chip

#endif
