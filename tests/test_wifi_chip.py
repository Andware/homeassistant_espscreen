"""The Wi-Fi co-processor of an ESP32-P4 screen (components/smart_display/wifi_chip.h).

A Guition V3 whose C6 runs ESP-Hosted 2.3.2 missed esp_hosted's own 10 ms reset, so after every software restart the
screen kept restarting until its power went off (the bench board, 2026-10-26). The core holds the C6 in reset from the
first moment of every start until esp_hosted starts it, on the pin and at the levels of the board's esp32_hosted block
(through esp_hosted's sdkconfig), and says what the C6 runs in a diagnostic sensor that the troubleshooting report
reads."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHIP = ROOT / 'components' / 'smart_display' / 'wifi_chip.cpp'
CORE = ROOT / 'packages' / 'hardware' / 'esp32p4-core.yaml'
HARDWARE = ROOT / 'packages' / 'hardware'
REPORTS = ROOT / 'screen_manager' / 'app' / 'reports.py'


def held_part():
    """The part of wifi_chip.cpp that a build with esp_hosted on SDIO compiles."""
    text = CHIP.read_text()
    start = text.index('#if defined(USE_ESP32_HOSTED) && defined(CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE)')
    return text[start:text.rindex('\n#else')]


class TheHold(unittest.TestCase):
    def test_the_pin_and_its_levels_are_esp_hosteds_never_a_number(self):
        part = held_part()
        self.assertIn('static_cast<gpio_num_t>(CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE)', part)
        self.assertNotRegex(part, r'GPIO_NUM_\d+')
        self.assertRegex(part, r'#ifdef CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW\s+constexpr uint32_t HELD = 1;'
                               r'\s+#else\s+constexpr uint32_t HELD = 0;')

    def test_it_holds_before_app_main_through_the_driver(self):
        part = held_part()
        hold = part[part.index('__attribute__((constructor)) void hold()'):]
        hold = hold[:hold.index('\n}')]
        calls = [hold.index(call) for call in ('gpio_reset_pin(RESET_PIN)', 'gpio_set_level(RESET_PIN, HELD)',
                                                'gpio_set_direction(RESET_PIN, GPIO_MODE_OUTPUT)')]
        self.assertEqual(calls, sorted(calls))
        self.assertNotIn('REG_WRITE', hold)

    def test_every_p4_board_names_the_pin_and_its_level(self):
        blocks = [p for p in HARDWARE.glob('*.yaml') if re.search(r'^esp32_hosted:', p.read_text(), re.M)]
        self.assertGreaterEqual(len(blocks), 2)
        for path in blocks:
            block = path.read_text().split('\nesp32_hosted:', 1)[1].split('\n\n', 1)[0]
            self.assertIn('reset_pin:', block, path.name)
            self.assertIn('active_high:', block, path.name)


class TheFirmware(unittest.TestCase):
    def test_the_c6_is_asked_once_with_wifi_up(self):
        part = held_part()
        firmware = part[part.index('std::string firmware()'):]
        self.assertLess(firmware.index('if (!told.empty())'), firmware.index('is_connected()'))
        self.assertLess(firmware.index('is_connected()'), firmware.index('esp_hosted_get_coprocessor_fwversion'))
        self.assertEqual(part.count('esp_hosted_get_coprocessor_fwversion('), 1)
        self.assertIn('"Wi-Fi co-processor: %s"', firmware)
        self.assertIn('ESP_LOGD(TAG, "Wi-Fi co-processor held in reset until the host was ready (GPIO%d)"', firmware)

    def test_every_p4_screen_has_the_sensor_and_the_report_reads_it(self):
        core = CORE.read_text()
        self.assertRegex(core, r'name: "Wi-Fi co-processor"\s+entity_category: diagnostic\s+'
                               r"lambda: 'return \{wifi_chip::firmware\(\)\};'")
        self.assertIn("'wi-fi co-processor'", REPORTS.read_text())


if __name__ == '__main__':
    unittest.main()
