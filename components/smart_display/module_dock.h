// The dock (module dock, tier full; docs/MODULES.md). Up from the bottom edge opens a drawer along
// the bottom of the glass with every page as a small picture and its name under it, two rows of them; a tap on one
// goes there, a tap above the drawer or a drag down closes it. A lite screen goes home on that swipe, as it always
// did, and compiles none of this.
//
// Each direction has one meaning: up and down open and close the drawer, sideways leafs through the pages when they
// do not all fit, one drawer's width at a time with a dot for each. The pictures are as large as the glass allows:
// all pages side by side where they fit at a readable size (a ten-inch glass holds eight), fewer and leafing on a
// small one. The drawer rises as an iOS sheet does and settles with a little overshoot, its pages following one after
// the other; only position and opacity move, never a scale, which the software renderer would pay for by the pixel.
//
// The pictures are drawn from the model, not from the cards: a kept page is drawn only while it is on the glass
// (docs/KEPT_PAGES.md), so a page's picture is its grid, each tile a rounded block in the colour its tile has now.
// Included by runtime_tiles.h where the touch code needs it. ESPHome also puts every header of the component into
// main.cpp on its own: there the site is not defined and this file is empty.
#if defined(SD_MOD_DOCK) && defined(SMART_DISPLAY_MODULE_SITE) && !defined(SMART_DISPLAY_MODULE_DOCK)
#define SMART_DISPLAY_MODULE_DOCK
namespace dock {
using namespace runtime_tiles;

inline lv_obj_t *root = nullptr;
// The drawer on its way out: it slides down, then goes. A new swipe up meanwhile deletes it at once.
inline lv_obj_t *leaving = nullptr;
inline lv_obj_t *drawer = nullptr, *strip = nullptr, *dots = nullptr;
inline int drawer_h = 0, views = 1, glass_w = 0;

constexpr uint32_t RISE_MS = 380, ITEM_MS = 260, ITEM_STEP_MS = 30, DIM_MS = 220, LEAVE_MS = 220, BACK_MS = 240;
constexpr lv_opa_t DIM = LV_OPA_50;

inline bool open() { return root != nullptr; }

inline void set_opa(void *o, int32_t v) { lv_obj_set_style_opa(static_cast<lv_obj_t *>(o), static_cast<lv_opa_t>(v), 0); }
inline void set_bg_opa(void *o, int32_t v) { lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(o), static_cast<lv_opa_t>(v), 0); }
inline void set_rise(void *o, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(o), v, 0); }
inline void animate(lv_obj_t *o, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t ms, uint32_t delay,
                    lv_anim_path_cb_t path, lv_anim_completed_cb_t done = nullptr) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, exec);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, path);
  if (done) lv_anim_set_completed_cb(&a, done);
  exec(o, from);
  lv_anim_start(&a);
}
inline void gone(lv_anim_t *a) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  if (o == leaving) leaving = nullptr;
  lv_obj_delete_async(o);
}
inline void drop_leaving() {
  if (!leaving) return;
  lv_anim_delete(leaving, nullptr);
  if (auto *d = lv_obj_get_child(leaving, 0)) lv_anim_delete(d, nullptr);
  lv_obj_delete(leaving);
  leaving = nullptr;
}
// Down and off the glass from wherever the drawer is now (a drag may have taken it part of the way), the dim with it.
inline void close() {
  if (!root) return;
  drop_leaving();
  leaving = root;
  root = nullptr;
  lv_obj_remove_flag(leaving, LV_OBJ_FLAG_CLICKABLE);
  lv_anim_delete(leaving, nullptr);
  if (drawer) {
    lv_anim_delete(drawer, nullptr);
    lv_obj_remove_flag(drawer, LV_OBJ_FLAG_CLICKABLE);
    animate(drawer, set_rise, lv_obj_get_style_translate_y(drawer, LV_PART_MAIN), drawer_h, LEAVE_MS, 0, lv_anim_path_ease_in);
  }
  animate(leaving, set_bg_opa, lv_obj_get_style_bg_opa(leaving, LV_PART_MAIN), LV_OPA_TRANSP, LEAVE_MS, 0, lv_anim_path_ease_in, gone);
  drawer = strip = dots = nullptr;
}

// A finger that moves down over the drawer takes it along; let go past a quarter of its height, or with a flick, it
// closes, else it settles back. Sideways is the strip's, which LVGL scrolls; a drag never picks a page.
struct Drag {
  bool down = false, dragging = false, decided = false;
  int start_x = 0, start_y = 0, last_y = 0, offset = 0, speed = 0;
  uint32_t last_ms = 0;
};
inline Drag drag;
inline lv_point_t point() {
  lv_point_t p{0, 0};
  if (auto *indev = lv_indev_active()) lv_indev_get_point(indev, &p);
  return p;
}
inline void follow(int offset) {
  drag.offset = std::max(0, offset);
  lv_obj_set_style_translate_y(drawer, drag.offset, 0);
  const int left = drawer_h > 0 ? std::max(0, drawer_h - drag.offset) * 100 / drawer_h : 0;
  lv_obj_set_style_bg_opa(root, static_cast<lv_opa_t>(DIM * left / 100), 0);
}
inline void on_drag(lv_event_t *e) {
  if (!root || !drawer) return;
  const auto code = lv_event_get_code(e);
  const lv_point_t p = point();
  if (code == LV_EVENT_PRESSED) {
    drag = Drag{};
    drag.down = true;
    drag.start_x = p.x; drag.start_y = drag.last_y = p.y;
    drag.last_ms = lv_tick_get();
    return;
  }
  if (!drag.down) return;
  if (code == LV_EVENT_PRESSING) {
    const int dx = p.x - drag.start_x, dy = p.y - drag.start_y;
    if (!drag.decided && (std::abs(dx) > ui::mm(2) || std::abs(dy) > ui::mm(2))) {
      drag.decided = true;
      // Down, more than sideways, and the strip not already scrolling: the drawer's.
      auto *indev = lv_indev_active();
      drag.dragging = dy > 0 && dy > std::abs(dx) && !(indev && lv_indev_get_scroll_obj(indev));
      if (drag.dragging) {
        lv_anim_delete(drawer, nullptr);
        lv_anim_delete(root, nullptr);
        if (strip) lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
      }
    }
    if (drag.dragging) {
      const uint32_t now = lv_tick_get();
      if (now != drag.last_ms) drag.speed = (p.y - drag.last_y) * 1000 / static_cast<int>(now - drag.last_ms);
      drag.last_y = p.y; drag.last_ms = now;
      follow(dy);
    }
    return;
  }
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    drag.down = false;
    if (!drag.dragging) return;
    // Where the finger let go counts too: a panel read seldom may have reported only the start of the drag.
    if (p.y - drag.start_y > drag.offset) follow(p.y - drag.start_y);
    if (strip) lv_obj_add_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    // A flick is some 15 cm a second, whatever the glass.
    if (drag.offset > drawer_h / 4 || drag.speed > ui::mm(150)) {
      ESP_LOGI("touch", "dock: dragged away");
      close();
      return;
    }
    ESP_LOGI("touch", "dock: drag of %d px back (%d px/s)", drag.offset, drag.speed);
    animate(drawer, set_rise, drag.offset, 0, BACK_MS, 0, lv_anim_path_ease_out);
    animate(root, set_bg_opa, lv_obj_get_style_bg_opa(root, LV_PART_MAIN), DIM, BACK_MS, 0, lv_anim_path_ease_out);
  }
}

inline void pick(lv_event_t *e) {
  if (drag.dragging) return;
  const int page = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  ESP_LOGI("touch", "dock: page %d", page + 1);
  // The page goes on the glass above the drawer, and the drawer slides off it.
  go_to_page(page, false);
  close();
}
inline void backdrop(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) close();
}
// A dot per view, the one in view a short bar, centred under the pages (by hand: this LVGL has no flex layout).
inline void paint_dots(int view) {
  if (!dots) return;
  const int n = static_cast<int>(lv_obj_get_child_count(dots)), dot = ui::mm(1) + ui::mm(1) / 2, bar = ui::mm(4);
  int x = (glass_w - (n - 1) * (dot + dot) - bar) / 2;
  for (int i = 0; i < n; ++i) {
    auto *d = lv_obj_get_child(dots, i);
    const bool on = i == view;
    lv_obj_set_pos(d, x, 0);
    lv_obj_set_width(d, on ? bar : dot);
    lv_obj_set_style_bg_color(d, theme::color(on ? theme::INK : theme::TRACK), 0);
    x += (on ? bar : dot) + dot;
  }
}
inline void strip_scrolled(lv_event_t *e) {
  auto *s = static_cast<lv_obj_t *>(lv_event_get_target(e));
  const int w = lv_obj_get_width(s);
  if (w > 0) paint_dots((lv_obj_get_scroll_x(s) + w / 2) / w);
}

inline lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t colour) {
  auto *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, std::max(1, w), std::max(1, h));
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, theme::rgb(colour), 0);
  return o;
}

// One page in small: the top bar as a dot and two strokes, then its tiles on the grid with the page's margins.
inline void picture(lv_obj_t *parent, int page, int w, int h, const std::array<Placement, TILES_MAX> &placement) {
  auto *glass = block(parent, 0, 0, w, h, std::max(2, w / 24), theme::hex(theme::PAGE));
  const int margin = std::max(2, w * 3 / 100), gap = std::max(1, w * 2 / 100), bar = std::max(4, h * 11 / 100);
  const uint32_t stroke = theme::hex(theme::TRACK);
  block(glass, margin, margin, bar * 3 / 5, bar * 3 / 5, LV_RADIUS_CIRCLE, stroke);
  block(glass, margin + bar, margin + bar / 5, w / 6, bar / 5, bar, stroke);
  block(glass, w - margin - w / 12, margin + bar / 5, w / 12, bar / 5, bar, stroke);
  const int top = margin + bar, cols = static_cast<int>(grid.columns), rows = static_cast<int>(grid.rows);
  const int cell_w = (w - 2 * margin - (cols - 1) * gap) / cols, cell_h = (h - top - margin - (rows - 1) * gap) / rows;
  const int radius = std::max(1, std::min(cell_w, cell_h) / 6);
  for (size_t i = 0; i < model.count && i < grid.max_tiles(); ++i) {
    if (placement[i].page != page) continue;
    const auto &t = model.tiles[i];
    const int col = placement[i].slot % cols, row = placement[i].slot / cols;
    const int cs = static_cast<int>(t.column_span()), rs = static_cast<int>(t.row_span());
    const uint32_t colour = t.active() ? theme::mix(tile_controls::accent(t), theme::hex(theme::CARD), 110) : theme::hex(theme::CARD);
    block(glass, margin + col * (cell_w + gap), top + row * (cell_h + gap), cs * cell_w + (cs - 1) * gap,
          rs * cell_h + (rs - 1) * gap, radius, colour);
  }
}

inline void build() {
  drop_leaving();
  root = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(root);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(root, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(root, theme::color(theme::SCRIM), 0);
  lv_obj_add_event_cb(root, backdrop, LV_EVENT_CLICKED, nullptr);

  const int W = overlay_card::screen_width(), H = overlay_card::screen_height();
  glass_w = W;
  const int pages = static_cast<int>(page_count());
  const int pad = ui::mm(4), gap = ui::mm(3), inset = ui::mm(1) + ui::mm(1) / 2, line = lv_font_get_line_height(label_font);
  const int grab = ui::mm(6), dots_h = ui::mm(4), room_w = W - 2 * pad;
  // The columns of one drawer's width: every page in two rows when each picture keeps at least 13 mm, no more columns
  // than that, and so few that two rows stay within half the glass.
  auto item_w_of = [&](int cols) { return (room_w - (cols - 1) * gap) / cols; };
  auto item_h_of = [&](int cols) { return inset + (item_w_of(cols) - 2 * inset) * H / W + ui::mm(1) + line + inset; };
  // One row when every page fits in it, two otherwise; the dots when they do not all fit in two.
  auto rows_of = [&](int cols) { return pages > cols ? 2 : 1; };
  auto drawer_of = [&](int cols) {
    return grab + rows_of(cols) * item_h_of(cols) + (rows_of(cols) - 1) * gap + (pages > 2 * cols ? dots_h : 0) + pad;
  };
  const int most = std::max(1, (room_w + gap) / (ui::mm(13) + gap));
  int cols = std::min(std::max(1, (pages + 1) / 2), most);
  while (cols < most && drawer_of(cols) > H / 2) ++cols;
  const int per_view = 2 * cols;
  views = (pages + per_view - 1) / per_view;
  const int item_w = item_w_of(cols), item_h = item_h_of(cols), pic_w = item_w - 2 * inset, pic_h = pic_w * H / W;
  const int rows = rows_of(cols), band = rows * item_h + (rows - 1) * gap;
  drawer_h = drawer_of(cols);
  // The drawer reaches below the glass by its corner and the overshoot's way, so the bottom never shows a gap.
  const int below = std::max(ui::mm(5), drawer_h / 8);

  drawer = lv_obj_create(root);
  lv_obj_remove_style_all(drawer);
  lv_obj_remove_flag(drawer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(drawer, 0, H - drawer_h);
  lv_obj_set_size(drawer, W, drawer_h + below);
  lv_obj_set_style_radius(drawer, ui::mm(6), 0);
  lv_obj_set_style_bg_opa(drawer, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(drawer, theme::color(theme::RAISED), 0);
  for (auto code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(drawer, on_drag, code, nullptr);
  block(drawer, (W - ui::mm(10)) / 2, (grab - ui::mm(1)) / 2, ui::mm(10), ui::mm(1), LV_RADIUS_CIRCLE, theme::hex(theme::TRACK));

  strip = lv_obj_create(drawer);
  lv_obj_remove_style_all(strip);
  lv_obj_add_flag(strip, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_pos(strip, 0, grab);
  lv_obj_set_size(strip, W, band);
  lv_obj_set_scroll_dir(strip, LV_DIR_HOR);
  lv_obj_set_scroll_snap_x(strip, LV_SCROLL_SNAP_START);
  lv_obj_add_flag(strip, LV_OBJ_FLAG_SCROLL_ONE);
  lv_obj_set_scrollbar_mode(strip, LV_SCROLLBAR_MODE_OFF);
  if (views <= 1) lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(strip, strip_scrolled, LV_EVENT_SCROLL_END, nullptr);

  std::array<Placement, TILES_MAX> placement;
  place(model, placement);
  const int current = shown_page ? *shown_page : 0;
  // Each view is a drawer's width, so a sideways swipe stops at the next one; its pages fill it row by row.
  std::vector<lv_obj_t *> items;
  for (int v = 0; v < views; ++v) {
    auto *view = lv_obj_create(strip);
    lv_obj_remove_style_all(view);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(view, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_set_pos(view, v * W, 0);
    lv_obj_set_size(view, W, band);
    for (int k = 0; k < per_view && v * per_view + k < pages; ++k) {
      const int page = v * per_view + k, col = k % cols, row = k / cols;
      auto *item = lv_obj_create(view);
      lv_obj_remove_style_all(item);
      lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(item, LV_OBJ_FLAG_EVENT_BUBBLE);
      lv_obj_set_pos(item, pad + col * (item_w + gap), row * (item_h + gap));
      lv_obj_set_size(item, item_w, item_h);
      lv_obj_set_style_radius(item, ui::mm(3), 0);
      lv_obj_set_style_bg_color(item, theme::color(theme::CARD_PRESSED), 0);
      lv_obj_set_style_bg_opa(item, page == current ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
      lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_PRESSED);
      lv_obj_add_event_cb(item, pick, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(page)));
      auto *pic = lv_obj_create(item);
      lv_obj_remove_style_all(pic);
      lv_obj_remove_flag(pic, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(pic, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_pos(pic, inset, inset);
      lv_obj_set_size(pic, pic_w, pic_h);
      picture(pic, page, pic_w, pic_h, placement);
      auto *name = lv_label_create(item);
      lv_label_set_text(name, model.title_of(page).c_str());
      lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
      // One line, its end dotted: LVGL dots a label only when its height is fixed.
      lv_obj_set_size(name, item_w - 2 * inset, line);
      lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_font(name, label_font, 0);
      lv_obj_set_style_text_color(name, theme::color(theme::INK), 0);
      lv_obj_set_pos(name, inset, inset + pic_h + ui::mm(1));
      items.push_back(item);
    }
  }
  const int first_view = std::min(current / per_view, views - 1);
  if (views > 1) {
    dots = lv_obj_create(drawer);
    lv_obj_remove_style_all(dots);
    lv_obj_remove_flag(dots, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dots, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(dots, W, dots_h);
    lv_obj_set_pos(dots, 0, grab + band + (dots_h - ui::mm(1) - ui::mm(1) / 2) / 2);
    for (int v = 0; v < views; ++v) {
      auto *d = block(dots, 0, 0, ui::mm(1) + ui::mm(1) / 2, ui::mm(1) + ui::mm(1) / 2, LV_RADIUS_CIRCLE, theme::hex(theme::TRACK));
      lv_obj_add_flag(d, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
    paint_dots(first_view);
    lv_obj_scroll_to_x(strip, first_view * W, LV_ANIM_OFF);
  }

  // In: the glass dims, the drawer rises from the bottom edge and settles, its pages follow in reading order.
  animate(root, set_bg_opa, LV_OPA_TRANSP, DIM, DIM_MS, 0, lv_anim_path_ease_out);
  animate(drawer, set_rise, drawer_h, 0, RISE_MS, 0, lv_anim_path_overshoot);
  for (size_t i = 0; i < items.size(); ++i) {
    const int page = static_cast<int>(i);
    const uint32_t delay = 80 + ITEM_STEP_MS * static_cast<uint32_t>(std::abs(page - first_view * per_view));
    animate(items[i], set_rise, ui::mm(3), 0, ITEM_MS, delay, lv_anim_path_ease_out);
    animate(items[i], set_opa, LV_OPA_TRANSP, LV_OPA_COVER, ITEM_MS, delay, lv_anim_path_ease_out);
  }
}

inline void show() {
  static bool hooked = false;
  if (!hooked) {
    hooked = true;
    // Every card closing (go_home, the settings swipe, standby) takes the drawer with it, and while it is open the
    // screen counts as away, so "Back to page 1" closes it in time.
    screen_hooks::cards_closed().push_back(close);
    screen_hooks::away().push_back(open);
  }
  settings_screen::close();
  if (dismiss) dismiss();
  build();
}
}  // namespace dock
#endif
