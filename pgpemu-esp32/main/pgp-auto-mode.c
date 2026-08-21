#include "pgp-auto-mode.h"

bool pgp_auto_mode_is_valid(pgp_auto_mode_t mode)
{
	return mode >= PGP_AUTO_MODE_BOTH && mode < PGP_AUTO_MODE_COUNT;
}

pgp_auto_mode_t pgp_auto_mode_next(pgp_auto_mode_t mode)
{
	if (!pgp_auto_mode_is_valid(mode)) {
		return PGP_AUTO_MODE_BOTH;
	}
	return (pgp_auto_mode_t)((mode + 1) % PGP_AUTO_MODE_COUNT);
}

bool pgp_auto_mode_allows_event(pgp_auto_mode_t mode, pgp_led_event_t event)
{
	switch (mode) {
	case PGP_AUTO_MODE_BOTH:
		return event == PGP_LED_EVENT_POKEMON_ENCOUNTER ||
		       event == PGP_LED_EVENT_POKESTOP_ENCOUNTER;
	case PGP_AUTO_MODE_CATCH_ONLY:
		return event == PGP_LED_EVENT_POKEMON_ENCOUNTER;
	case PGP_AUTO_MODE_STOPS_ONLY:
		return event == PGP_LED_EVENT_POKESTOP_ENCOUNTER;
	case PGP_AUTO_MODE_PAUSED:
	case PGP_AUTO_MODE_COUNT:
	default:
		return false;
	}
}

const char *pgp_auto_mode_label(pgp_auto_mode_t mode)
{
	switch (mode) {
	case PGP_AUTO_MODE_BOTH:
		return "AUTO BOTH";
	case PGP_AUTO_MODE_CATCH_ONLY:
		return "CATCH ONLY";
	case PGP_AUTO_MODE_STOPS_ONLY:
		return "STOPS ONLY";
	case PGP_AUTO_MODE_PAUSED:
		return "PAUSED";
	case PGP_AUTO_MODE_COUNT:
	default:
		return "AUTO BOTH";
	}
}
