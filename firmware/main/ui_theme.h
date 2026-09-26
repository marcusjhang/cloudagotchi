/*
 * The one place UI colours, spacing and radii are named. Build the surface
 * scale UP from a near-black AMOLED background so lit pixels stay low (burn-in
 * and battery); everything the screen draws reads these tokens, nothing sets a
 * raw hex inline. See the `lvgl-ui` skill for the conventions.
 *
 * Palette: "cozy night" - a deep navy sky with stars and a moon, muted candy
 * tiles. Warm and child-friendly without lighting up the whole panel.
 */
#pragma once

#include "lvgl.h"

/* ---- surfaces: stepped up from near-black -------------------------------- */
#define UI_BG               lv_color_hex(0x080B18)  // screen
#define UI_SURFACE          lv_color_hex(0x1B2740)
#define UI_SURFACE_PRESSED  lv_color_hex(0x27385A)
#define UI_SURFACE_OFF      lv_color_hex(0x10182A)
#define UI_PANEL            lv_color_hex(0x101830)
#define UI_BORDER           lv_color_hex(0x2A3A5C)
#define UI_TRACK            lv_color_hex(0x22304A)  // need-bar track
#define UI_GROUND           lv_color_hex(0x16324A)  // ground band
#define UI_STAR             lv_color_hex(0x6B7BB5)
#define UI_MOON             lv_color_hex(0xFFF3C4)

/* ---- ink ---------------------------------------------------------------- */
#define UI_TEXT             lv_color_hex(0xF2F5FF)
#define UI_TEXT_DIM         lv_color_hex(0x8E9AC0)
#define UI_ON_COLOR         lv_color_hex(0xFFFFFF)  // icon on a candy tile
#define UI_ON_WARN          lv_color_hex(0x2A1E00)

/* ---- muted candy accents (tiles, bars, call) ----------------------------- */
#define UI_C_FEED           lv_color_hex(0xE8A15C)  // warm orange
#define UI_C_PLAY           lv_color_hex(0xE88AB0)  // pink
#define UI_C_CLEAN          lv_color_hex(0x5AA9E6)  // sky
#define UI_C_MED            lv_color_hex(0x67C57A)  // green
#define UI_ACCENT           lv_color_hex(0x9A7BFF)

/* ---- geometry ----------------------------------------------------------- */
#define UI_RADIUS           14
#define UI_RADIUS_LG        24
#define UI_PAD              12
#define UI_GAP              8
