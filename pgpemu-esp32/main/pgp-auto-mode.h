#pragma once

#include <stdbool.h>

#include "pgp-events.h"

typedef enum {
	PGP_AUTO_MODE_BOTH = 0,
	PGP_AUTO_MODE_CATCH_ONLY,
	PGP_AUTO_MODE_STOPS_ONLY,
	PGP_AUTO_MODE_PAUSED,
	PGP_AUTO_MODE_COUNT,
} pgp_auto_mode_t;

bool pgp_auto_mode_is_valid(pgp_auto_mode_t mode);
pgp_auto_mode_t pgp_auto_mode_next(pgp_auto_mode_t mode);
bool pgp_auto_mode_allows_event(pgp_auto_mode_t mode, pgp_led_event_t event);
const char *pgp_auto_mode_label(pgp_auto_mode_t mode);
