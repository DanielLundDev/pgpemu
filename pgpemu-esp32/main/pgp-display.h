#pragma once

#include <stdbool.h>

#include "pgp-auto-mode.h"

bool pgp_display_init(void);
void pgp_display_set_connected(bool connected);
void pgp_display_set_auto_mode(pgp_auto_mode_t mode);
void pgp_display_pokemon_caught(void);
void pgp_display_pokemon_fled(void);
void pgp_display_pokestop_spun(void);
