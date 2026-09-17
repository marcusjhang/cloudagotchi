/* The pet's state in NVS: one versioned blob, ~150 bytes. */
#pragma once

#include <stdbool.h>

#include "pet.h"

bool persist_load(pet_t *out);       // false if absent or a different schema
bool persist_save(const pet_t *p);
void persist_erase(void);
