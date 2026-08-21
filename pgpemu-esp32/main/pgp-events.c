#include "pgp-events.h"

pgp_led_event_t pgp_parse_led_event(const uint8_t *buffer, size_t length)
{
	if (buffer == NULL || length < 4) {
		return PGP_LED_EVENT_NONE;
	}

	size_t number_of_patterns = buffer[3] & 0x1f;
	if (number_of_patterns == 0 || number_of_patterns > (length - 4) / 3) {
		return PGP_LED_EVENT_NONE;
	}

	int count_ballshake = 0;
	int count_red = 0;
	int count_green = 0;
	int count_blue = 0;
	int count_yellow = 0;
	int count_off = 0;
	int count_notoff = 0;

	for (size_t i = 0; i < number_of_patterns; i++) {
		const uint8_t *pattern = &buffer[4 + 3 * i];
		uint8_t red = pattern[1] & 0x0f;
		uint8_t green = (pattern[1] >> 4) & 0x0f;
		uint8_t blue = pattern[2] & 0x0f;

		if (!red && !green && !blue) {
			count_off++;
			continue;
		}

		count_notoff++;
		if (i <= 9 && red && green && blue) {
			count_ballshake++;
		}

		if (red && !green && !blue) {
			count_red++;
		} else if (!red && green && !blue) {
			count_green++;
		} else if (!red && !green && blue) {
			count_blue++;
		} else if (red && green && !blue) {
			count_yellow++;
		}
	}

	if ((count_green == count_notoff || count_yellow == count_notoff) && count_notoff > 0) {
		return PGP_LED_EVENT_POKEMON_ENCOUNTER;
	}
	if (count_blue == count_notoff && count_notoff > 0) {
		return PGP_LED_EVENT_POKESTOP_ENCOUNTER;
	}
	if (count_ballshake && count_blue && count_green) {
		return PGP_LED_EVENT_POKEMON_CAUGHT;
	}
	if (count_ballshake && count_red) {
		return PGP_LED_EVENT_POKEMON_FLED;
	}
	if (count_red && count_green && count_blue && !count_off) {
		return PGP_LED_EVENT_POKESTOP_SPUN;
	}

	return PGP_LED_EVENT_NONE;
}
