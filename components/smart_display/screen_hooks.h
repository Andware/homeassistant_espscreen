#pragma once
// The screen's moments as lists (docs/PROFILES.md, "Hooks"): what a feature of the board (the camera) or a plugin wants
// to do at a moment of the core, added to a list instead of written into a substitution of packages/core.yaml. A
// substitution holds one piece of code, so a second feature that set it would silently replace the first; a list adds
// up. A feature adds its own at boot (features/camera.yaml), a plugin through plugin_host.cpp.
//
// The choices of the board's hardware (which touch panel, how the backlight dims, what a feature does at boot) stay
// substitutions: exactly one package of a board gives each.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>

namespace screen_hooks {

// Every 250 ms, after runtime_tiles::tick(), with millis().
inline std::vector<std::function<void(uint32_t)>> &tick() {
  static std::vector<std::function<void(uint32_t)>> list;
  return list;
}
// The cards closed (runtime_tiles::dismiss): Back, standby, Back to page 1, another card.
inline std::vector<std::function<void()>> &cards_closed() {
  static std::vector<std::function<void()>> list;
  return list;
}
// Something on the glass that keeps the settings page from opening over it (a camera full screen): any true says so.
inline std::vector<std::function<bool()>> &keeps_settings_closed() {
  static std::vector<std::function<bool()>> list;
  return list;
}
// Something on the glass that counts as away from page 1, so "Back to page 1" closes it in time: any true says so.
inline std::vector<std::function<bool()>> &away() {
  static std::vector<std::function<bool()>> list;
  return list;
}
// A tap the screen took: the touch filter let it through to a tile, a key or a button (screen_input::TouchGuard), or
// it landed on a row of the settings, Back or the pager (settings_screen); never the repeat of a key that is held.
// For feedback a finger can hear or feel; it runs inside the touch event, so what it does must be quick.
inline std::vector<std::function<void()>> &touched() {
  static std::vector<std::function<void()>> list;
  return list;
}
inline void run_touched() {
  for (auto &f : touched()) f();
}
// A finger came down on something that takes a tap or a drag (plugin_host.cpp, LVGL's press of the input device): at
// once, before the screen knows whether it becomes a tap, so a click can answer the finger instead of the action.
inline std::vector<std::function<void()>> &pressed() {
  static std::vector<std::function<void()>> list;
  return list;
}
// A dragged slider or dial passed a step (screen_input::step_of): once for every step it passed, at most two at once.
inline std::vector<std::function<void()>> &stepped() {
  static std::vector<std::function<void()>> list;
  return list;
}
inline void run_stepped() {
  for (auto &f : stepped()) f();
}
// Where a dragged control is: `control` names it (its object), `step` is the step its value is on now. A control's first
// call after a finger landed (drag_begin) only says where it starts, so a slider that jumps to the finger is no run of
// steps; after that every step passed reaches stepped(), two at most, as a detent clicks once however fast it turns.
// LVGL's sliders and dials come here through plugin_host.cpp; a control the core drags itself (a player's seek bar) calls
// it from its own handler. Without a plugin that listens, nothing is kept.
struct DragSteps {
  const void *control = nullptr;
  int step = 0;
};
inline DragSteps &drag_steps() {
  static DragSteps steps;
  return steps;
}
inline void drag_begin() { drag_steps().control = nullptr; }
inline void drag_at(const void *control, int step) {
  if (stepped().empty()) return;
  auto &d = drag_steps();
  if (d.control != control) {
    d.control = control;
    d.step = step;
    return;
  }
  const int passed = std::min(2, std::abs(step - d.step));
  d.step = step;
  for (int i = 0; i < passed; ++i) run_stepped();
}
// When no press is heard: the push that wakes a dimmed screen, a calibration (core.yaml sets it at boot).
inline bool (*presses_quiet)() = nullptr;
// An alert is about to show, after the settings page closed and before its card is made.
inline std::vector<std::function<void()>> &alert_show() {
  static std::vector<std::function<void()>> list;
  return list;
}

// Run a list: in screen_hooks.cpp, so the lambdas of packages/core.yaml that call them hold one call each.
void run_tick(uint32_t now_ms);
void run_cards_closed();
bool settings_kept_closed();
bool is_away();
void run_alert_show();

}  // namespace screen_hooks
