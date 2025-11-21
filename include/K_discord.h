#pragma once

#ifdef K_DISCORD
#include <cdiscord.h>
#endif

void discord_init(), discord_update(), discord_teardown();

void update_discord_status(const char*);
