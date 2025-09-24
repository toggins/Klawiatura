#include "K_audio.h"
#include "K_file.h"
#include "K_log.h"
#include "K_memory.h"
#include "K_string.h"

#include <fmod_errors.h>

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

static StTinyMap* sounds = NULL;
static StTinyMap* tracks = NULL;

void audio_init() {
	FMOD_Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_TTY, NULL, NULL);
	FMOD_RESULT result = FMOD_System_Create(&speaker, FMOD_VERSION);
	if (result != FMOD_OK)
		FATAL("FMOD_System_Create fail: %s", FMOD_ErrorString(result));

	result = FMOD_System_Init(speaker, MAX_SOUNDS, FMOD_INIT_NORMAL, NULL);
	if (result != FMOD_OK)
		FATAL("FMOD_System_Init fail: %s", FMOD_ErrorString(result));

	uint32_t version, buildnumber;
	FMOD_System_GetVersion(speaker, &version, &buildnumber);
	INFO("FMOD version: %u.%02u.%02u - Build %u", (version >> 16) & 0xFFFF, (version >> 8) & 0xFF, version & 0xFF,
		buildnumber);

	FMOD_System_GetMasterChannelGroup(speaker, &master_group);
	FMOD_System_CreateChannelGroup(speaker, "sound", &sound_group);
	FMOD_System_CreateChannelGroup(speaker, "music", &music_group);

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
	FMOD_BOOL state_playing = false;
	FMOD_ChannelGroup_IsPlaying(state_group, &state_playing);
	FMOD_ChannelGroup_SetMute(generic_group, state_playing);

	FMOD_System_Update(speaker);
}

void audio_teardown() {
	FreeTinyMap(sounds);
	FreeTinyMap(tracks);

	FMOD_System_Release(speaker);
}

void set_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(master_group, volume);
}

void set_sound_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(sound_group, volume);
}

void set_music_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(music_group, volume);
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
	if (get_sound(name) != NULL)
		return;

	Sound sound = {0};

	const char* file = find_data_file(fmt("data/sounds/%s.*", name), ".json");
	FMOD_SOUND* data = NULL;
	FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESAMPLE, NULL, &data);
	if (result != FMOD_OK) {
		WTF("Sound \"%s\" fail: %s", name, FMOD_ErrorString(result));
		return;
	}

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
	if (get_track(name) != NULL)
		return;

	Track track = {0};

	const char* file = find_data_file(fmt("data/music/%s.*", name), ".json");
	FMOD_SOUND* data = NULL;
	FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESTREAM | FMOD_ACCURATETIME, NULL, &data);
	if (result != FMOD_OK) {
		WTF("Track \"%s\" fail: %s", name, FMOD_ErrorString(result));
		return;
	}

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

// =======
// GENERIC
// =======

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

	stop_generic_tracks();

	FMOD_CHANNEL* channel = NULL;
	FMOD_System_PlaySound(speaker, track->stream, generic_music_group, true, &channel);
	FMOD_Channel_SetMode(channel, (flags & PLAY_LOOPING ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
	FMOD_Channel_SetPaused(channel, false);
}

void stop_generic_sounds() {
	FMOD_ChannelGroup_Stop(generic_sound_group);
}

void stop_generic_tracks() {
	FMOD_ChannelGroup_Stop(generic_music_group);
}
