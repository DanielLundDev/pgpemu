#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
	PGP_LED_EVENT_NONE = 0,
	PGP_LED_EVENT_POKEMON_ENCOUNTER,
	PGP_LED_EVENT_POKEMON_CAUGHT,
	PGP_LED_EVENT_POKEMON_FLED,
	PGP_LED_EVENT_POKESTOP_SPUN,
} pgp_led_event_t;

pgp_led_event_t pgp_parse_led_event(const uint8_t *buffer, size_t length);
