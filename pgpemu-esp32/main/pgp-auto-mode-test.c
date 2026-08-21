#include <assert.h>
#include <string.h>

#include "pgp-auto-mode.h"

int main(void)
{
	assert(pgp_auto_mode_next(PGP_AUTO_MODE_BOTH) == PGP_AUTO_MODE_CATCH_ONLY);
	assert(pgp_auto_mode_next(PGP_AUTO_MODE_CATCH_ONLY) == PGP_AUTO_MODE_STOPS_ONLY);
	assert(pgp_auto_mode_next(PGP_AUTO_MODE_STOPS_ONLY) == PGP_AUTO_MODE_PAUSED);
	assert(pgp_auto_mode_next(PGP_AUTO_MODE_PAUSED) == PGP_AUTO_MODE_BOTH);
	assert(pgp_auto_mode_next(PGP_AUTO_MODE_COUNT) == PGP_AUTO_MODE_BOTH);

	assert(pgp_auto_mode_allows_event(PGP_AUTO_MODE_BOTH,
					  PGP_LED_EVENT_POKEMON_ENCOUNTER));
	assert(pgp_auto_mode_allows_event(PGP_AUTO_MODE_BOTH,
					  PGP_LED_EVENT_POKESTOP_ENCOUNTER));
	assert(pgp_auto_mode_allows_event(PGP_AUTO_MODE_CATCH_ONLY,
					  PGP_LED_EVENT_POKEMON_ENCOUNTER));
	assert(!pgp_auto_mode_allows_event(PGP_AUTO_MODE_CATCH_ONLY,
					   PGP_LED_EVENT_POKESTOP_ENCOUNTER));
	assert(pgp_auto_mode_allows_event(PGP_AUTO_MODE_STOPS_ONLY,
					  PGP_LED_EVENT_POKESTOP_ENCOUNTER));
	assert(!pgp_auto_mode_allows_event(PGP_AUTO_MODE_STOPS_ONLY,
					   PGP_LED_EVENT_POKEMON_ENCOUNTER));
	assert(!pgp_auto_mode_allows_event(PGP_AUTO_MODE_PAUSED,
					   PGP_LED_EVENT_POKEMON_ENCOUNTER));
	assert(!pgp_auto_mode_allows_event(PGP_AUTO_MODE_BOTH,
					   PGP_LED_EVENT_POKEMON_CAUGHT));

	assert(pgp_auto_mode_is_valid(PGP_AUTO_MODE_BOTH));
	assert(!pgp_auto_mode_is_valid(PGP_AUTO_MODE_COUNT));
	assert(strcmp(pgp_auto_mode_label(PGP_AUTO_MODE_BOTH), "AUTO BOTH") == 0);
	assert(strcmp(pgp_auto_mode_label(PGP_AUTO_MODE_CATCH_ONLY), "CATCH ONLY") == 0);
	assert(strcmp(pgp_auto_mode_label(PGP_AUTO_MODE_STOPS_ONLY), "STOPS ONLY") == 0);
	assert(strcmp(pgp_auto_mode_label(PGP_AUTO_MODE_PAUSED), "PAUSED") == 0);

	return 0;
}
