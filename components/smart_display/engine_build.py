"""The picture engine, the glass and the live streams a build gets (docs/PICTURE_ARCHITECTURE.md, section 3.9; docs/
PLATFORM_PLAN.md 3.2): keyed on the chip and on what the build has (LVGL, PSRAM), never on a board, so a new board has
them without a line in its file. Free of ESPHome, so tests/test_live_view.py checks it; __init__.py applies it.

Three things, each a define the C++ builds under, and one may come without the others:

- The engine (picture_engine.h): what turns a picture's bytes into pixels. PICTURE_ENGINE_P4 is the ESP32-P4's hardware
  JPEG decoder and its PPA, which also scales, turns and copies pixels (picture_engine_p4.cpp, with ppa_guard.cpp
  between). Without it, the BMP rows of picture_fetch.
- The glass (glass.h): a frame buffer a task may write past LVGL. GLASS_DSI is a P4's MIPI-DSI panel as ESP-IDF makes it
  (glass_dsi.cpp); whether a P4's display is one is found at run time (glass::direct).
- The live streams (live_view.h): USE_LIVE_VIEW, a camera's pictures written onto the glass as they come. They need an
  engine that copies pixels and a glass to copy them onto: on a P4 alone today.

A chip with another decoder or panel kind is a row here, with its own files beside these; nothing in picture_fetch,
live_view, live_spots or runtime_tiles changes for it.
"""

ENGINES = {
    "p4": {
        "defines": ("PICTURE_ENGINE_P4", "GLASS_DSI", "USE_LIVE_VIEW"),
        # The linker wraps the engine and the glass need: the panel picked up as ESP-IDF makes it and every PPA client's
        # bursts (glass_dsi.cpp), and the guard of every turned copy (ppa_guard.cpp).
        "wraps": ("esp_lcd_new_panel_dpi", "ppa_register_client", "ppa_do_scale_rotate_mirror"),
        "components": (),
    },
    "software": {"defines": (), "wraps": (), "components": ()},
}


def picture_engine(variant, has_lvgl, has_psram=False):
    """The engine a build gets: 'p4' for an ESP32-P4 with LVGL, else 'software'. `variant` as ESPHome names it,
    lowercase; `has_psram` for an engine that needs PSRAM to decode into."""
    if not has_lvgl:
        return "software"
    if variant == "esp32p4":
        return "p4"
    return "software"
