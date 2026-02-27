#include <SDL3/SDL_stdinc.h>

#include "K_audio.h"
#include "K_chat.h"
#include "K_input.h"
#include "K_log.h"
#include "K_misc.h"
#include "K_net.h"
#include "K_string.h"
#include "K_tick.h"

static const float chat_fs = 18.f;
static char chat_message[100] = {0};
static struct {
	char text[sizeof(chat_message)];
	float lifetime;
} chat_hist[8] = {0};

void push_chat_message(int from, const char* text, int size) {
	if (size <= 0)
		return;

	for (int i = entries(chat_hist) - 1; i >= 1; i--)
		chat_hist[i] = chat_hist[i - 1];

	static char buf[sizeof(chat_message)] = {0};
	SDL_strlcpy(buf, text, SDL_min(sizeof(chat_message), size + 1));
	const char* line = fmt("%s: %s", get_peer_name(from), buf);
	INFO("%s", line);

	SDL_strlcpy(chat_hist[0].text, line, sizeof(chat_message));
	chat_hist[0].lifetime = 6 * TICKRATE;
	play_generic_sound("chat");
}

static void send_chat_message() {
	if (typing_what() || !SDL_strlen(chat_message))
		return;

	char* data = net_buffer();
	*(PacketType*)data = PT_CHAT;
	const int ssize = (int)SDL_strlcpy(
		data + sizeof(PacketType), chat_message, SDL_min(sizeof(chat_message), NET_BUFFER_SIZE));

	const int psize = (int)sizeof(PacketType) + ssize;
	for (int i = 0; i < MAX_PEERS; i++)
		if (NutPunch_PeerAlive(i))
			NutPunch_SendReliably(PCH_LOBBY, i, data, psize);

	push_chat_message(NutPunch_LocalPeer(), chat_message, sizeof(chat_message));
	chat_message[0] = 0;
}

static void draw_chat_hist() {
	const float y = SCREEN_HEIGHT - 12.f - (1.5 * chat_fs);
	const Uint8 a = (typing_what() == chat_message) ? 255 : 192;
	batch_reset();
	for (int i = 0; i < entries(chat_hist); i++) {
		if (chat_hist[i].lifetime <= 0.f)
			break;
		const float fade = chat_hist[i].lifetime / TICKRATE;
		batch_pos(B_XYZ(12.f, y - (i * chat_fs), -10000.f));
		batch_color(B_ALPHA(SDL_min(1.f, fade) * a)), batch_align(B_BOTTOM_LEFT);
		batch_string("main", chat_fs, chat_hist[i].text);
	}
}

static void draw_chat_message() {
	if (typing_what() != chat_message)
		return;
	batch_pos(B_XYZ(12.f, SCREEN_HEIGHT - 12.f, -10000.f)), batch_color(B_WHITE), batch_align(B_BOTTOM_LEFT);
	batch_string("main", chat_fs, fmt("> %s%s", chat_message, SDL_fmodf(totalticks(), 30.f) < 16.f ? "|" : ""));
}

void tick_chat_hist() {
	const float delta = dt();
	for (int i = 0; i < entries(chat_hist); i++) {
		if (chat_hist[i].lifetime > 0.f)
			chat_hist[i].lifetime -= delta;
		if (chat_hist[i].lifetime <= 0.f) {
			if (i >= (entries(chat_hist) - 1))
				SDL_zero(chat_hist[i]);
			else {
				for (int j = i; j < entries(chat_hist) - 1; j++)
					chat_hist[j] = chat_hist[j + 1];
				SDL_zero(chat_hist[entries(chat_hist) - 1]);
			}
		}
	}
}

void handle_chat_inputs() {
	if (typing_what() || !NutPunch_IsReady())
		return;

	const Bool paused = game_exists() && game_paused();
	if (kb_pressed(KB_CHAT) && !paused)
		start_typing(chat_message, sizeof(chat_message));
	else if (typing_input_confirmed())
		send_chat_message();
	else
		chat_message[0] = 0;
}

void draw_chat() {
	draw_chat_hist();
	draw_chat_message();
}

void purge_chat() {
	SDL_zero(chat_hist);
}
