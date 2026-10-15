// The page switcher (module page_switcher, tier full; docs/MODULES.md). Up from the bottom edge opens a window over
// the glass with every page as a small picture and its name under it; a tap on one goes there, a tap beside the window
// or another swipe up closes it. A lite screen goes home on that swipe, as it always did, and compiles none of this.
//
// The pictures are drawn from the model, not from the cards: a kept page is drawn only while it is on the glass
// (docs/KEPT_PAGES.md), so a page's picture is its grid, each tile a rounded block in the colour its tile has now.
// Included by runtime_tiles.h where the touch code needs it. ESPHome also puts every header of the component into
// main.cpp on its own: there the site is not defined and this file is empty.
#if defined(SD_MOD_PAGE_SWITCHER) && defined(SMART_DISPLAY_MODULE_SITE) && !defined(SMART_DISPLAY_MODULE_PAGE_SWITCHER)
#define SMART_DISPLAY_MODULE_PAGE_SWITCHER
namespace page_switcher {
using namespace runtime_tiles;

inline lv_obj_t *root = nullptr;
// The pages one view holds; more pages than that take a row of dots to pick the view by.
inline int view = 0;

inline bool open() { return root != nullptr; }
inline void close() {
  if (!root) return;
  lv_obj_delete(root);
  root = nullptr;
}
inline void build();

inline void pick(lv_event_t *e) {
  const int page = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  ESP_LOGI("touch", "page switcher: page %d", page + 1);
  close();
  go_to_page(page, false);
}
inline void pick_view(lv_event_t *e) {
  view = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  build();
}
inline void backdrop(lv_event_t *e) {
  if (lv_event_get_target(e) == lv_event_get_current_target(e)) close();
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
  if (root) lv_obj_clean(root);
  else {
    root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, theme::color(theme::SCRIM), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_50, 0);
    lv_obj_add_event_cb(root, backdrop, LV_EVENT_CLICKED, nullptr);
  }
  const int W = overlay_card::screen_width(), H = overlay_card::screen_height();
  const bool lying = W >= H;
  const int cols = lying ? 3 : 2, rows = lying ? 2 : 3, per_view = cols * rows;
  const int pages = static_cast<int>(page_count()), views = (pages + per_view - 1) / per_view;
  view = std::clamp(view, 0, std::max(0, views - 1));
  const int shown = std::min(per_view, pages - view * per_view), used_rows = (std::min(shown, per_view) + cols - 1) / cols;

  const int pad = ui::mm(4), gap = ui::mm(2), inset = ui::mm(2), label_h = lv_font_get_line_height(label_font) + ui::mm(2);
  const int panel_w = std::min(W - 2 * ui::mm(4), ui::px(1100));
  const int item_w = (panel_w - 2 * pad - (cols - 1) * gap) / cols, pic_w = item_w - 2 * inset, pic_h = pic_w * H / W;
  const int item_h = inset + pic_h + label_h, dots_h = views > 1 ? ui::mm(5) : 0;
  const int panel_h = std::min(H - 2 * ui::mm(4), 2 * pad + dots_h + used_rows * item_h + (used_rows - 1) * gap);

  auto *panel = lv_obj_create(root);
  lv_obj_remove_style_all(panel);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(panel, panel_w, panel_h);
  lv_obj_center(panel);
  lv_obj_set_style_radius(panel, ui::mm(5), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(panel, theme::color(theme::RAISED), 0);

  for (int v = 0; views > 1 && v < views; ++v) {
    const int dot = ui::mm(2), wide = v == view ? dot * 3 : dot, step = ui::mm(4);
    const int x = (panel_w - (views - 1) * step - dot * 3) / 2 + v * step + (v > view ? dot * 2 : 0);
    auto *d = block(panel, x, pad, wide, dot, LV_RADIUS_CIRCLE, theme::hex(v == view ? theme::INK : theme::TRACK));
    lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(d, ui::mm(2));
    lv_obj_add_event_cb(d, pick_view, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(v)));
  }

  std::array<Placement, TILES_MAX> placement;
  place(model, placement);
  const int current = shown_page ? *shown_page : 0;
  for (int k = 0; k < shown; ++k) {
    const int page = view * per_view + k, col = k % cols, row = k / cols;
    auto *item = lv_obj_create(panel);
    lv_obj_remove_style_all(item);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(item, pad + col * (item_w + gap), pad + dots_h + row * (item_h + gap));
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
    lv_obj_set_size(name, item_w - 2 * inset, lv_font_get_line_height(label_font));
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(name, label_font, 0);
    lv_obj_set_style_text_color(name, theme::color(theme::INK), 0);
    lv_obj_set_pos(name, inset, inset + pic_h + ui::mm(1));
  }
}

inline void show() {
  static bool hooked = false;
  if (!hooked) {
    hooked = true;
    // Every card closing (go_home, the settings swipe, standby) takes the window with it, and while it is open the
    // screen counts as away, so "Back to page 1" closes it in time.
    screen_hooks::cards_closed().push_back(close);
    screen_hooks::away().push_back(open);
  }
  settings_screen::close();
  if (dismiss) dismiss();
  // The view that holds the page on the glass (six a view, three by two lying down and two by three standing up).
  view = shown_page ? *shown_page / 6 : 0;
  build();
}
}  // namespace page_switcher
#endif
