#include <fmod_errors.h>

#include "K_audio.h"
#include "K_file.h"
#include "K_game.h"
#include "K_log.h"
#include "K_memory.h"
#include "K_string.h"
#include "K_video.h"

static FMOD_SYSTEM* speaker = NULL;

// Internal noise
static FMOD_CHANNELGROUP* master_group = NULL;
static FMOD_CHANNELGROUP* sound_group = NULL;
static FMOD_CHANNELGROUP* music_group = NULL;

// Generic noise
static FMOD_CHANNELGROUP* generic_group = NULL;
static FMOD_CHANNELGROUP* generic_sound_group = NULL;
static FMOD_CHANNELGROUP* generic_music_group = NULL;

// Game state noise
static FMOD_CHANNELGROUP* state_group = NULL;
static FMOD_CHANNELGROUP* state_sound_group = NULL;
static FMOD_CHANNELGROUP* state_music_group = NULL;

AudioState audio_state = {0};
static FMOD_CHANNEL* music_channels[TS_SIZE] = {NULL};

static StTinyMap* sounds = NULL;
static StTinyMap* tracks = NULL;

void audio_init() {
	FMOD_Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_TTY, NULL, NULL);
	FMOD_RESULT result = FMOD_System_Create(&speaker, FMOD_VERSION);
	EXPECT(result == FMOD_OK, "FMOD_System_Create fail: %s", FMOD_ErrorString(result));

	result = FMOD_System_Init(speaker, MAX_SOUNDS * 2, FMOD_INIT_NORMAL, NULL);
	EXPECT(result == FMOD_OK, "FMOD_System_Init fail: %s", FMOD_ErrorString(result));

	uint32_t version, buildnumber;
	FMOD_System_GetVersion(speaker, &version, &buildnumber);
	INFO("FMOD version: %u.%02u.%02u - Build %u", (version >> 16) & 0xFFFF, (version >> 8) & 0xFF, version & 0xFF,
		buildnumber);

	FMOD_System_GetMasterChannelGroup(speaker, &master_group);
	FMOD_System_CreateChannelGroup(speaker, "sound", &sound_group);
	FMOD_System_CreateChannelGroup(speaker, "music", &music_group);
	FMOD_ChannelGroup_SetVolume(music_group, 0.5f);

	FMOD_System_CreateChannelGroup(speaker, "generic", &generic_group);
	FMOD_System_CreateChannelGroup(speaker, "generic_sound", &generic_sound_group);
	FMOD_System_CreateChannelGroup(speaker, "generic_music", &generic_music_group);
	FMOD_ChannelGroup_AddGroup(sound_group, generic_sound_group, true, NULL);
	FMOD_ChannelGroup_AddGroup(state_group, generic_sound_group, true, NULL);
	FMOD_ChannelGroup_AddGroup(music_group, generic_music_group, true, NULL);
	FMOD_ChannelGroup_AddGroup(state_group, generic_music_group, true, NULL);

	FMOD_System_CreateChannelGroup(speaker, "state", &state_group);
	FMOD_System_CreateChannelGroup(speaker, "state_sound", &state_sound_group);
	FMOD_System_CreateChannelGroup(speaker, "state_music", &state_music_group);
	FMOD_ChannelGroup_AddGroup(sound_group, state_sound_group, true, NULL);
	FMOD_ChannelGroup_AddGroup(state_group, state_sound_group, true, NULL);
	FMOD_ChannelGroup_AddGroup(music_group, state_music_group, true, NULL);
	FMOD_ChannelGroup_AddGroup(state_group, state_music_group, true, NULL);

	sounds = NewTinyMap();
	tracks = NewTinyMap();
}

void audio_update() {
	FMOD_ChannelGroup_SetMute(master_group, !window_focused());
	FMOD_ChannelGroup_SetMute(generic_music_group, game_exists());

	FMOD_System_Update(speaker);
}

void audio_teardown() {
	FreeTinyMap(sounds);
	FreeTinyMap(tracks);

	FMOD_System_Release(speaker);
}

float get_volume() {
	float volume = 1;
	FMOD_ChannelGroup_GetVolume(master_group, &volume);
	return volume;
}

void set_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(master_group, volume);
}

float get_sound_volume() {
	float volume = 1;
	FMOD_ChannelGroup_GetVolume(sound_group, &volume);
	return volume;
}

void set_sound_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(sound_group, volume);
}

float get_music_volume() {
	float volume = 0.5f;
	FMOD_ChannelGroup_GetVolume(music_group, &volume);
	return volume;
}

void set_music_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(music_group, volume);
}

void move_ears(const float pos[2]) {
	FMOD_System_Set3DListenerAttributes(speaker, 0, &((FMOD_VECTOR){pos[0], pos[1], -320}), &((FMOD_VECTOR){0}),
		&((FMOD_VECTOR){0, 0, -1}), &((FMOD_VECTOR){0, -1, 0}));
}

// =====
// STATE
// =====

void start_audio_state() {
	nuke_audio_state();
}

void tick_audio_state() {
	for (size_t i = 0; i < TS_SIZE; i++) {
		TrackObject* track = &audio_state.tracks[i];
		if (track->track == NULL)
			continue;

		track->offset += 20;
		if (!(track->flags & PLAY_LOOPING) && track->offset >= track->track->length)
			stop_state_track(i);
	}

	for (size_t i = 0; i < MAX_SOUNDS; i++) {
		SoundObject* sound = &audio_state.sounds[i];
		if (sound->sound == NULL)
			continue;

		sound->offset += 20;
		if (sound->offset >= sound->sound->length)
			sound->sound = NULL;
	}
}

void save_audio_state(AudioState* as) {
	SDL_memcpy(as, &audio_state, sizeof(audio_state));
}

void load_audio_state(const AudioState* as) {
	for (size_t i = 0; i < TS_SIZE; i++) {
		const TrackObject* load_track = &as->tracks[i];
		TrackObject* cur_track = &audio_state.tracks[i];
		if (cur_track->track != load_track->track || cur_track->flags != load_track->flags) {
			stop_state_track(i);
			if (load_track->track == NULL
				|| (!(load_track->flags & PLAY_LOOPING) && load_track->track != NULL
					&& load_track->offset >= load_track->track->length))
				continue;

			FMOD_System_PlaySound(
				speaker, load_track->track->stream, state_music_group, true, &(music_channels[i]));
			FMOD_Channel_SetMode(music_channels[i],
				((load_track->flags & PLAY_LOOPING) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF)
					| FMOD_ACCURATETIME);
			FMOD_Channel_SetMute(music_channels[i], i < as->top_track);
			FMOD_Channel_SetPosition(music_channels[i], load_track->offset, FMOD_TIMEUNIT_MS);
			FMOD_Channel_SetPaused(music_channels[i], false);

			if (i > as->top_track)
				for (size_t j = 0; j < i; j++)
					if (music_channels[j] != NULL)
						FMOD_Channel_SetMute(music_channels[j], true);
		}
	}

	SDL_memcpy(&audio_state, as, sizeof(audio_state));

	FMOD_ChannelGroup_Stop(state_sound_group);
	for (size_t i = 0; i < MAX_SOUNDS; i++) {
		const SoundObject* sound = &audio_state.sounds[i];
		if (sound->sound == NULL || sound->offset >= sound->sound->length)
			continue;

		FMOD_CHANNEL* channel = NULL;
		FMOD_System_PlaySound(speaker, sound->sound->sound, state_group, true, &channel);
		if (sound->flags & PLAY_PAN) {
			FMOD_Channel_SetMode(channel, FMOD_3D | FMOD_3D_LINEARROLLOFF);
			FMOD_Channel_Set3DMinMaxDistance(channel, 320, 640);
			FMOD_Channel_Set3DAttributes(
				channel, &((FMOD_VECTOR){sound->pos[0], sound->pos[1], 0}), &((FMOD_VECTOR){0}));
		}
		FMOD_Channel_SetPosition(channel, sound->offset, FMOD_TIMEUNIT_MS);
		FMOD_Channel_SetPaused(channel, false);
	}
}

void nuke_audio_state() {
	FMOD_ChannelGroup_Stop(state_group);
	SDL_memset(&audio_state, 0, sizeof(audio_state));
	SDL_memset(&music_channels, 0, sizeof(music_channels));
}

// ======
// ASSETS
// ======

static void nuke_sound(void* ptr) {
	Sound* sound = ptr;
	FMOD_Sound_Release(sound->sound);
	SDL_free(sound->name);
}

void load_sound(const char* name) {
	if (!name || !*name || get_sound(name))
		return;

	Sound sound = {0};

	const char* file = find_data_file(fmt("data/sounds/%s.*", name), ".json");
	ASSUME(file, "Sound \"%s\" not found", name);

	FMOD_SOUND* data = NULL;
	FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESAMPLE, NULL, &data);
	ASSUME(result == FMOD_OK, "Sound \"%s\" fail: %s", name, FMOD_ErrorString(result));

	sound.name = SDL_strdup(name);
	sound.sound = data;
	FMOD_Sound_GetLength(data, &(sound.length), FMOD_TIMEUNIT_MS);

	StMapPut(sounds, long_key(name), &sound, sizeof(sound))->cleanup = nuke_sound;
}

const Sound* get_sound(const char* name) {
	return StMapGet(sounds, long_key(name));
}

static void nuke_track(void* ptr) {
	Track* track = ptr;
	FMOD_Sound_Release(track->stream);
	SDL_free(track->name);
}

void load_track(const char* name) {
	if (!name || !*name || get_track(name))
		return;

	Track track = {0};

	const char* file = find_data_file(fmt("data/music/%s.*", name), ".json");
	ASSUME(file, "Track \"%s\" not found", name);

	FMOD_SOUND* data = NULL;
	FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESTREAM | FMOD_ACCURATETIME, NULL, &data);
	ASSUME(result == FMOD_OK, "Track \"%s\" fail: %s", name, FMOD_ErrorString(result));

	track.name = SDL_strdup(name);
	track.stream = data;
	FMOD_Sound_GetLength(data, &(track.length), FMOD_TIMEUNIT_MS);

	file = find_data_file(fmt("data/music/%s.json", name), NULL);
	if (file != NULL) {
		yyjson_doc* json = yyjson_read_file(file, JSON_READ_FLAGS, NULL, NULL);
		if (json != NULL) {
			yyjson_val* root = yyjson_doc_get_root(json);
			if (yyjson_is_obj(root)) {
				unsigned int loop_start = yyjson_get_uint(yyjson_obj_get(root, "loop_start"));
				unsigned int loop_end = yyjson_get_uint(yyjson_obj_get(root, "loop_end"));
				if (loop_end <= 0)
					loop_end = track.length;
				FMOD_Sound_SetLoopPoints(
					data, loop_start, FMOD_TIMEUNIT_MS, loop_end, FMOD_TIMEUNIT_MS);
			}
			yyjson_doc_free(json);
		}
	}

	StMapPut(tracks, long_key(name), &track, sizeof(track))->cleanup = nuke_track;
}

const Track* get_track(const char* name) {
	return StMapGet(tracks, long_key(name));
}

// ==============
// GENERIC SOUNDS
// ==============

void play_generic_sound(const char* name) {
	const Sound* sound = get_sound(name);
	if (sound == NULL) {
		WARN("Unknown sound \"%s\"", name);
		return;
	}

	FMOD_System_PlaySound(speaker, sound->sound, generic_sound_group, false, NULL);
}

void play_generic_track(const char* name, PlayFlags flags) {
	const Track* track = get_track(name);
	if (track == NULL) {
		WARN("Unknown track \"%s\"", name);
		return;
	}

	FMOD_ChannelGroup_Stop(generic_music_group);

	FMOD_CHANNEL* channel = NULL;
	FMOD_System_PlaySound(speaker, track->stream, generic_music_group, true, &channel);
	FMOD_Channel_SetMode(channel, (flags & PLAY_LOOPING ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
	FMOD_Channel_SetPaused(channel, false);
}

// ============
// STATE SOUNDS
// ============

void play_state_sound(const char* name) {
	const Sound* sound = get_sound(name);
	if (sound == NULL) {
		WARN("Unknown sound \"%s\"", name);
		return;
	}

	SoundObject* sobj = &audio_state.sounds[audio_state.next_sound];
	sobj->sound = sound;
	sobj->offset = 0;
	sobj->flags = 0;

	FMOD_CHANNEL* channel = NULL;
	FMOD_System_PlaySound(speaker, sound->sound, state_sound_group, false, &channel);

	audio_state.next_sound = (audio_state.next_sound + 1) % MAX_SOUNDS;
}

void play_state_sound_at(const char* name, const float pos[2]) {
	const Sound* sound = get_sound(name);
	if (sound == NULL) {
		WARN("Unknown sound \"%s\"", name);
		return;
	}

	SoundObject* sobj = &audio_state.sounds[audio_state.next_sound];
	sobj->sound = sound;
	sobj->offset = 0;

	sobj->flags = PLAY_PAN;
	sobj->pos[0] = pos[0];
	sobj->pos[1] = pos[1];

	FMOD_CHANNEL* channel = NULL;
	FMOD_System_PlaySound(speaker, sound->sound, state_sound_group, true, &channel);
	FMOD_Channel_SetMode(channel, FMOD_3D | FMOD_3D_LINEARROLLOFF);
	FMOD_Channel_Set3DMinMaxDistance(channel, 320, 640);
	FMOD_Channel_Set3DAttributes(channel, &((FMOD_VECTOR){pos[0], pos[1], 0}), &((FMOD_VECTOR){0}));
	FMOD_Channel_SetPaused(channel, false);

	audio_state.next_sound = (audio_state.next_sound + 1) % MAX_SOUNDS;
}

void play_state_track(TrackSlots slot, const char* name, PlayFlags flags) {
	const Track* track = get_track(name);
	if (track == NULL) {
		WARN("Unknown track \"%s\"", name);
		return;
	}

	TrackObject* tobj = &audio_state.tracks[slot];
	tobj->track = track;
	tobj->offset = 0;
	tobj->flags = flags;

	if (music_channels[slot] != NULL)
		FMOD_Channel_Stop(music_channels[slot]);
	FMOD_System_PlaySound(speaker, track->stream, state_music_group, true, &(music_channels[slot]));
	FMOD_Channel_SetMode(
		music_channels[slot], ((flags & PLAY_LOOPING) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
	FMOD_Channel_SetMute(music_channels[slot], slot < audio_state.top_track);
	FMOD_Channel_SetPaused(music_channels[slot], false);

	if (slot > audio_state.top_track) {
		for (size_t i = 0; i < slot; i++)
			if (music_channels[i] != NULL)
				FMOD_Channel_SetMute(music_channels[i], true);
		audio_state.top_track = slot;
	}
}

void stop_state_track(TrackSlots slot) {
	TrackObject* track = &audio_state.tracks[slot];
	if (track->track == NULL)
		return;
	track->track = NULL;

	if (music_channels[slot] != NULL) {
		FMOD_Channel_Stop(music_channels[slot]);
		music_channels[slot] = NULL;
	}

	if (audio_state.top_track == slot && slot > 0) {
		int i = slot - 1;
		while (i >= 0) {
			if (music_channels[i] != NULL) {
				FMOD_Channel_SetMute(music_channels[i], false);
				audio_state.top_track = i;
				break;
			}
			--i;
		}
	}
}
