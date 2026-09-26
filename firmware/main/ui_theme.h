/*
 * The one place UI colours, spacing and radii are named. Build the surface
 * scale UP from a true-black AMOLED background so lit pixels stay low (burn-in
 * and battery); everything the screen draws reads these tokens, nothing sets
 * a raw hex inline. See the `lvgl-ui` skill for the conventions.
 */
#pragma once

#include "lvgl.h"

/* ---- surfaces: stepped up from black ------------------------------------ */
#define UI_BG               lv_color_hex(0x000000)  // screen: pixels off
#define UI_SURFACE          lv_color_hex(0x121A22)
#define UI_SURFACE_PRESSED  lv_color_hex(0x1D2836)
#define UI_SURFACE_OFF      lv_color_hex(0x0C1218)  // disabled
#define UI_PANEL            lv_color_hex(0x0B1420)  // overlays
#define UI_BORDER           lv_color_hex(0x2A3644)

/* ---- ink and accents ---------------------------------------------------- */
#define UI_TEXT             lv_color_hex(0xE8EEF4)
#define UI_TEXT_DIM         lv_color_hex(0x9AA9B8)
#define UI_ACCENT           lv_color_hex(0x2EC4B6)  // active / checked
#define UI_SUCCESS          lv_color_hex(0x9CC959)
#define UI_WARN             lv_color_hex(0xFFD166)
#define UI_DANGER           lv_color_hex(0xFF5A5F)

/* ---- soft, child-friendly pieces ---------------------------------------- */
#define UI_TRACK            lv_color_hex(0x1C2836)  // bar track
#define UI_GROUND           lv_color_hex(0x0F1822)  // pedestal under the pet
#define UI_BUBBLE           lv_color_hex(0x17222E)  // caption speech bubble
#define UI_ON_WARN          lv_color_hex(0x2A1E00)  // dark ink on the warn toast

/* ---- geometry ----------------------------------------------------------- */
#define UI_RADIUS           12
#define UI_RADIUS_SM        8
#define UI_RADIUS_LG        18
#define UI_PAD              12
#define UI_GAP              8
#define UI_BTN_W            56
#define UI_BTN_H            62
#define UI_BAR_H            12
