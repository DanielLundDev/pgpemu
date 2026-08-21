#include <assert.h>
#include <stdint.h>

#include "pgp-events.h"

int main(void)
{
	static const uint8_t pokemon_encounter[] = {
		0, 0, 0, 3,
		16, 0xf0, 0x00,
		8, 0x00, 0x00,
		16, 0xf0, 0x00,
	};
	static const uint8_t pokestop_encounter[] = {
		0, 0, 0, 3,
		16, 0x00, 0x0f,
		8, 0x00, 0x00,
		16, 0x00, 0x0f,
	};
	static const uint8_t pokemon_caught[] = {
		0, 0, 0, 3,
		3, 0x88, 0x08,
		3, 0xf0, 0x00,
		3, 0x00, 0x0f,
	};
	static const uint8_t pokemon_fled[] = {
		0, 0, 0, 2,
		3, 0x88, 0x08,
		3, 0x0f, 0x00,
	};
	static const uint8_t pokestop_spun[] = {
		0, 0, 0, 3,
		3, 0x0f, 0x00,
		3, 0xf0, 0x00,
		3, 0x00, 0x0f,
	};
	static const uint8_t lights_off[] = {
		0, 0, 0, 1,
		10, 0x00, 0x00,
	};

	assert(pgp_parse_led_event(pokemon_encounter, sizeof(pokemon_encounter)) ==
	       PGP_LED_EVENT_POKEMON_ENCOUNTER);
	assert(pgp_parse_led_event(pokestop_encounter, sizeof(pokestop_encounter)) ==
	       PGP_LED_EVENT_POKESTOP_ENCOUNTER);
	assert(pgp_parse_led_event(pokemon_caught, sizeof(pokemon_caught)) ==
	       PGP_LED_EVENT_POKEMON_CAUGHT);
	assert(pgp_parse_led_event(pokemon_fled, sizeof(pokemon_fled)) ==
	       PGP_LED_EVENT_POKEMON_FLED);
	assert(pgp_parse_led_event(pokestop_spun, sizeof(pokestop_spun)) ==
	       PGP_LED_EVENT_POKESTOP_SPUN);
	assert(pgp_parse_led_event(lights_off, sizeof(lights_off)) == PGP_LED_EVENT_NONE);
	assert(pgp_parse_led_event(NULL, 0) == PGP_LED_EVENT_NONE);

	return 0;
}
