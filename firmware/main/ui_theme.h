/*
 * The one place UI colours, spacing and radii are named. Build the surface
 * scale UP from a true-black AMOLED background so lit pixels stay low (burn-in
 * and battery); everything the screen draws reads these tokens, nothing sets a
 * raw hex inline. See the `lvgl-ui` skill for the conventions.
 *
 * Palette: candy accents on black. Toy-like, high-contrast, child-friendly.
 */
#pragma once

#include "lvgl.h"

/* ---- surfaces: stepped up from black ------------------------------------ */
#define UI_BG               lv_color_hex(0x000000)  // screen: pixels off
#define UI_SURFACE          lv_color_hex(0x1B1E2B)
#define UI_SURFACE_PRESSED  lv_color_hex(0x2A2F42)
#define UI_SURFACE_OFF      lv_color_hex(0x141620)
#define UI_PANEL            lv_color_hex(0x141826)
#define UI_BORDER           lv_color_hex(0x2A2F42)

/* ---- ink ---------------------------------------------------------------- */
#define UI_TEXT             lv_color_hex(0xF2F5FF)
#define UI_TEXT_DIM         lv_color_hex(0x8E97AD)
#define UI_ON_COLOR         lv_color_hex(0xFFFFFF)  // icon on a candy tile
#define UI_ON_WARN          lv_color_hex(0x2A1E00)

/* ---- candy accents (tiles, hearts, call) -------------------------------- */
#define UI_C_FEED           lv_color_hex(0xFF9F45)  // orange
#define UI_C_PLAY           lv_color_hex(0xFF6FA5)  // pink
#define UI_C_CLEAN          lv_color_hex(0x45B7F0)  // sky
#define UI_C_MED            lv_color_hex(0x5BD66F)  // green
#define UI_C_LIGHT          lv_color_hex(0xFFD93D)  // sun
#define UI_HEART_FOOD       lv_color_hex(0xFFB44C)  // warm
#define UI_HEART_FUN        lv_color_hex(0xFF6FA5)  // pink
#define UI_HEART_EMPTY      lv_color_hex(0x323848)
#define UI_ACCENT           lv_color_hex(0x9A7BFF)  // lavender

/* ---- geometry ----------------------------------------------------------- */
#define UI_RADIUS           14
#define UI_RADIUS_LG        24
#define UI_PAD              12
#define UI_GAP              8
