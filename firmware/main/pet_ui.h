#pragma once

#include <stdint.h>

/** The pet's visible moods — driven by cloud state from article 3 onwards. */
typedef enum {
    PET_MOOD_HAPPY,
    PET_MOOD_NEUTRAL,
    PET_MOOD_SAD,
    PET_MOOD_SLEEPING,
} pet_mood_t;

/** What the human just did — reported to the cloud. */
typedef enum {
    PET_INTERACTION_PET,    // tapped the ghost
    PET_INTERACTION_FEED,   // picked a snack from the menu
    PET_INTERACTION_PLAY,   // shook the board
} pet_interaction_t;

typedef void (*pet_interaction_cb_t)(pet_interaction_t what);

/** Bring the pet to life on the AMOLED. Callback fires on every interaction. */
void pet_ui_start(pet_interaction_cb_t on_interaction);

/** Update the pet's stats (0–100 each) and mood. Safe to call from any task. */
void pet_ui_set_state(uint8_t hunger, uint8_t energy, uint8_t mood_value, pet_mood_t mood);

/** One-off reactions (sprite flashes). Safe to call from any task. */
void pet_ui_react_happy(void);    // happy face (tap, feed)
void pet_ui_react_startled(void); // wide eyes + "oh" (shake)

