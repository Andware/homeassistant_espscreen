# Tiers and modules

Not every board has room for every part of the UI. The boards with 4 MB of flash (the CYD first) are close to the end
of their update slot, while the others have 8 MB or more. Instead of checking per board what fits, every screen has a
**tier**, and the parts only the larger tier has room for are **modules**.

## The tier

The tier follows from the flash the firmware is built for, never from a board's name or its look:

| Tier | Flash | Boards |
|---|---|---|
| `lite` | less than 8 MB | every board that includes `packages/hardware/flash-4mb.yaml` |
| `full` | 8 MB or more | every other board |

`components/smart_display/__init__.py` reads `esp32: flash_size` while the firmware is built and defines
`SD_TIER_LITE` or `SD_TIER_FULL`, plus the define of every module that tier has. A new board gets its tier without a
line in its file. A host build (the renders) has no `esp32` block, so `tools/render/host.py` gives a 4 MB board
`smart_display: flash_mb: 4` and it builds lite there too. The look (`standard` or `compact`) is a separate thing: it sets sizes, not what the firmware has.

## A module

A module is listed in `components/smart_display/modules.py` with the lowest tier that has it and its define:

```python
MODULES = {
    "dock": {"min_tier": "full", "define": "SD_MOD_DOCK"},
}
```

Its code lives in `components/smart_display/module_<name>.h` (and a `module_<name>.cpp` if it needs one, wrapped
whole in its `#ifdef`). It stays in the component's own folder: ESPHome copies no folders inside it. The shared code
includes and calls it only inside `#ifdef SD_MOD_<NAME>`, with the behaviour a lite screen keeps in the other branch. A lite screen therefore compiles and links none of it, and its flash use does
not change.

`tools/generate_board_shapes.py` writes each board's `tier` and `modules` into `screen_manager/app/boards.json`, so
the add-on and the editor offer a module's options only on boards that have it.

## Adding a module

1. Add it to `MODULES` in `modules.py`.
2. Put its code in `components/smart_display/module_<name>.h`.
3. Reach it from the shared code only inside `#ifdef` of its define, and keep the lite behaviour in the other branch.
4. If the add-on or the editor offers something for it, check the board's `modules` in `boards.json`.
5. Run `tools/generate_board_shapes.py` and `tests/test_modules.py`, which fails on a module named outside its guard.

## The modules

- **dock** (full): up from the bottom edge opens the Dock, a small drawer at the bottom of the glass (at most 12 cm
  wide, the full width on a 4-inch glass) with every page as a small picture and its name, in one row or two.
  Sideways leafs through the pages when they do not all fit, a tap goes to a page, and a tap above the Dock or a drag
  down closes it. A page's **Show in Dock** setting (`navigation.showInDock` in the page document, on unless set
  off) leaves it out, and a page left out of swiping is never in it; its page message says `"dock": false` only for
  a page left out, which older firmware reads past. With every page left out, the swipe goes home. On lite, up from
  the bottom goes home.
