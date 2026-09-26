/*
 * The one place UI colours, spacing and radii are named. Everything the screen
 * draws reads these tokens; nothing sets a raw hex inline.
 *
 * Monochrome by design (see the reference: a big friendly face on black with no
 * chrome). White ink on true-black AMOLED - lowest power, no burn-in, and the
 * face carries the personality instead of colour. See the `lvgl-ui` skill.
 */
#pragma once

#include "lvgl.h"

#define UI_BG               lv_color_hex(0x000000)  // screen: pixels off
#define UI_INK              lv_color_hex(0xFFFFFF)  // eyes, icons, the active bar
#define UI_INK_DIM          lv_color_hex(0x8A8F98)  // the tiny status word
#define UI_SURFACE          lv_color_hex(0x121418)  // button fill
#define UI_SURFACE_PRESSED  lv_color_hex(0x23262C)
#define UI_SURFACE_OFF      lv_color_hex(0x0C0E11)
#define UI_PANEL            lv_color_hex(0x0E1013)
#define UI_LINE             lv_color_hex(0x2A2E35)  // hairlines / borders

#define UI_RADIUS           14
#define UI_RADIUS_LG        24
#define UI_PAD              12
#define UI_GAP              8
