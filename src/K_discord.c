#include "K_discord.h"
#include "K_log.h" // IWYU pragma: keep

void discord_init() {
#ifdef K_DISCORD
#else
	WARN("Discord RPC is unavailable for this build");
#endif
}

void discord_teardown() {
#ifdef K_DISCORD
#endif
}
