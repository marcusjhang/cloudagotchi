#pragma once
#include "lvgl.h"

// Face sprites (RGB565A8), all 210x162, aligned on a common canvas.
extern const lv_image_dsc_t face_happy;
extern const lv_image_dsc_t face_neutral;
extern const lv_image_dsc_t face_sad;
extern const lv_image_dsc_t face_sleeping;
extern const lv_image_dsc_t face_talk_1;
extern const lv_image_dsc_t face_talk_2;
extern const lv_image_dsc_t face_talk_3;

// Stat icons (RGB565A8, 28x28)
extern const lv_image_dsc_t icon_food;
extern const lv_image_dsc_t icon_energy;
extern const lv_image_dsc_t icon_heart;

// Blink frames: closed eyes + the mood's mouth (no zzz)
extern const lv_image_dsc_t face_blink_happy;
extern const lv_image_dsc_t face_blink_neutral;
extern const lv_image_dsc_t face_blink_sad;
