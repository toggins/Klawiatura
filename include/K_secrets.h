#pragma once

#include "K_input.h"

#define MAX_SECRET_INPUTS 5L

typedef enum {
	SECR_KEVIN,
	SECR_FRED,
	SECR_SIZE,
} SecretType;

typedef struct {
	const char* name;
	Keybind inputs[MAX_SECRET_INPUTS + 1];

	Bool* cmd;
	const char *sound, *track;

	size_t state;
	Uint8 type_time;
	Bool active;
} Secret;

void load_secrets(), update_secrets();
void draw_secrets(const float fade_red);

extern Secret SECRETS[SECR_SIZE];
