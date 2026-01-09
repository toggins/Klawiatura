#include <fmod_errors.h>

#include "K_audio.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_game.h"
#include "K_log.h"
#include "K_memory.h" // IWYU pragma: keep
#include "K_string.h"

extern ClientInfo CLIENT;

static FMOD_SYSTEM* speaker = NULL;

static FMOD_CHANNELGROUP *master_group = NULL, *sound_group = NULL, *music_group = NULL;
static FMOD_CHANNELGROUP *generic_sound_group = NULL, *generic_music_group = NULL;
static FMOD_CHANNELGROUP *state_sound_group = NULL, *state_music_group = NULL;

AudioState audio_state = {0};
static FMOD_CHANNEL *sound_channels[MAX_SOUNDS] = {NULL}, *music_channels[TS_SIZE] = {NULL};
static StTinyKey generic_track = 0;

static StTinyMap *sounds = NULL, *tracks = NULL;

void audio_init() {
	FMOD_Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_TTY, NULL, NULL);
	FMOD_RESULT result = FMOD_System_Create(&speaker, FMOD_VERSION);
	EXPECT(result == FMOD_OK, "FMOD_System_Create fail: %s", FMOD_ErrorString(result));

	result = FMOD_System_Init(speaker, MAX_SOUNDS * 2, FMOD_INIT_NORMAL, NULL);
	EXPECT(result == FMOD_OK, "FMOD_System_Init fail: %s", FMOD_ErrorString(result));

	Uint32 version = 0, buildnumber = 0;
	FMOD_System_GetVersion(speaker, &version, &buildnumber);
	INFO("FMOD version: %08x - Build %u", version, buildnumber);

	FMOD_System_GetMasterChannelGroup(speaker, &master_group);
	FMOD_System_CreateChannelGroup(speaker, "sound", &sound_group);
	FMOD_System_CreateChannelGroup(speaker, "music", &music_group);
	FMOD_ChannelGroup_SetVolume(music_group, 0.5f);

	FMOD_System_CreateChannelGroup(speaker, "generic_sound", &generic_sound_group);
	FMOD_System_CreateChannelGroup(speaker, "generic_music", &generic_music_group);
	FMOD_ChannelGroup_AddGroup(sound_group, generic_sound_group, TRUE, NULL);
	FMOD_ChannelGroup_AddGroup(music_group, generic_music_group, TRUE, NULL);

	FMOD_System_CreateChannelGroup(speaker, "state_sound", &state_sound_group);
	FMOD_System_CreateChannelGroup(speaker, "state_music", &state_music_group);
	FMOD_ChannelGroup_AddGroup(sound_group, state_sound_group, TRUE, NULL);
	FMOD_ChannelGroup_AddGroup(music_group, state_music_group, TRUE, NULL);

	sounds = NewTinyMap(), tracks = NewTinyMap();
}

void audio_update() {
	FMOD_ChannelGroup_SetMute(master_group, !(window_focused() || CLIENT.audio.background));
	FMOD_ChannelGroup_SetMute(generic_music_group, game_exists());
	FMOD_System_Update(speaker);
}

void audio_teardown() {
	FreeTinyMap(sounds), FreeTinyMap(tracks);
	FMOD_System_Release(speaker);
}

float get_volume() {
	float volume = 1.f;
	FMOD_ChannelGroup_GetVolume(master_group, &volume);
	return volume;
}

void set_volume(float volume) {
	FMOD_ChannelGroup_SetVolume(master_group, volume);
}

float get_sound_volume() {
	float volume = 1.f;
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

// ======
// ASSETS
// ======

static void nuke_sound(void* ptr) {
	Sound* sound = ptr;
	FMOD_Sound_Release(sound->sound);
	SDL_free(sound->base.name);
}

ASSET_SRC(sounds, Sound, sound);

void load_sound(const char* name, Bool transient) {
	if (!name || !*name || get_sound(name))
		return;

	Sound sound = {0};

	const char* file = find_data_file(fmt("data/sounds/%s.*", name), ".json");
	ASSUME(file, "Sound \"%s\" not found", name);

	FMOD_SOUND* data = NULL;
	FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESAMPLE, NULL, &data);
	ASSUME(result == FMOD_OK, "Sound \"%s\" fail: %s", name, FMOD_ErrorString(result));

	sound.base.name = SDL_strdup(name);
	sound.base.transient = transient;
	sound.sound = data;
	FMOD_Sound_GetLength(data, &(sound.length), FMOD_TIMEUNIT_MS);

	StMapPut(sounds, StHashStr(name), &sound, sizeof(sound))->cleanup = nuke_sound;
}

static void nuke_track(void* ptr) {
	Track* track = ptr;
	FMOD_Sound_Release(track->stream);
	SDL_free(track->base.name);
}

ASSET_SRC(tracks, Track, track);

void load_track(const char* name, Bool transient) {
	if (!name || !*name || get_track(name))
		return;

	Track track = {0};

	const char* file = find_data_file(fmt("data/music/%s.*", name), ".json");
	ASSUME(file, "Track \"%s\" not found", name);

	FMOD_SOUND* data = NULL;
	FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESTREAM | FMOD_ACCURATETIME, NULL, &data);
	ASSUME(result == FMOD_OK, "Track \"%s\" fail: %s", name, FMOD_ErrorString(result));

	track.base.name = SDL_strdup(name);
	track.base.transient = transient;
	track.stream = data;
	FMOD_Sound_GetLength(data, &(track.length), FMOD_TIMEUNIT_MS);

	file = find_data_file(fmt("data/music/%s.json", name), NULL);
	if (file == NULL)
		goto trk_no_json;
	yyjson_doc* json = yyjson_read_file(file, JSON_READ_FLAGS, NULL, NULL);
	if (json == NULL)
		goto trk_no_json;

	yyjson_val* root = yyjson_doc_get_root(json);
	if (!yyjson_is_obj(root))
		goto trk_no_obj;

	unsigned int loop_start = yyjson_get_uint(yyjson_obj_get(root, "loop_start"));
	unsigned int loop_end = yyjson_get_uint(yyjson_obj_get(root, "loop_end"));
	if (loop_end <= 0)
		loop_end = track.length;
	FMOD_Sound_SetLoopPoints(data, loop_start, FMOD_TIMEUNIT_MS, loop_end, FMOD_TIMEUNIT_MS);

trk_no_obj:
	yyjson_doc_free(json);

trk_no_json:
	StMapPut(tracks, StHashStr(name), &track, sizeof(track))->cleanup = nuke_track;
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

	FMOD_System_PlaySound(speaker, sound->sound, generic_sound_group, FALSE, NULL);
}

void play_generic_track(const char* name, PlayFlags flags) {
	const StTinyKey key = StHashStr(name);
	const Track* track = get_track_key(key);
	if (track == NULL) {
		WARN("Unknown track \"%s\"", name);
		return;
	}

	if (generic_track == key)
		return;
	stop_generic_track();

	FMOD_CHANNEL* channel = NULL;
	const FMOD_RESULT result = FMOD_System_PlaySound(speaker, track->stream, generic_music_group, TRUE, &channel);
	FMOD_Channel_SetMode(channel, (flags & PLAY_LOOPING ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
	FMOD_Channel_SetPaused(channel, FALSE);
	generic_track = (result == FMOD_OK) ? key : 0;
}

void stop_generic_track() {
	FMOD_ChannelGroup_Stop(generic_music_group);
	generic_track = 0;
}

// ============
// STATE SOUNDS
// ============

void start_audio_state() {
	nuke_audio_state();
}

void tick_audio_state() {
	for (size_t i = 0; i < MAX_SOUNDS; i++) {
		SoundObject* sound = &audio_state.sounds[i];
		const Sound* asset = get_sound_key(sound->sound_key);
		if (asset == NULL)
			continue;

		sound->offset += 20L;
		if (!(sound->flags & PLAY_LOOPING) && sound->offset >= asset->length)
			sound_channels[i] = NULL;
	}

	for (TrackSlots i = 0; i < (TrackSlots)TS_SIZE; i++) {
		TrackObject* track = &audio_state.tracks[i];
		const Track* asset = get_track_key(track->track_key);
		if (asset == NULL)
			continue;

		track->offset += 20L;
		if (!(track->flags & PLAY_LOOPING) && track->offset >= asset->length)
			stop_state_track(i);
	}
}

void save_audio_state(AudioState* as) {
	SDL_memcpy(as, &audio_state, sizeof(audio_state));
}

static void stop_state_sound(size_t);
void load_audio_state(const AudioState* as) {
	for (size_t i = 0; i < MAX_SOUNDS; i++) {
		const SoundObject* new_sound = &as->sounds[i];
		SoundObject* old_sound = &audio_state.sounds[i];
		if (old_sound->sound_key == new_sound->sound_key && old_sound->flags == new_sound->flags)
			continue;

		stop_state_sound(i);

		const Sound* new_asset = get_sound_key(new_sound->sound_key);
		if (new_asset == NULL || (!(new_sound->flags & PLAY_LOOPING) && new_sound->offset >= new_asset->length))
			continue;

		FMOD_System_PlaySound(speaker, new_asset->sound, state_sound_group, TRUE, &sound_channels[i]);

		if (new_sound->flags & PLAY_PAN) {
			FMOD_Channel_SetPan(sound_channels[i], new_sound->pos[0]);
		} else if (new_sound->flags & PLAY_POS) {
			FMOD_Channel_SetMode(sound_channels[i], FMOD_3D | FMOD_3D_LINEARROLLOFF);
			FMOD_Channel_Set3DMinMaxDistance(sound_channels[i], HALF_SCREEN_WIDTH, SCREEN_WIDTH);
			FMOD_Channel_Set3DAttributes(sound_channels[i],
				&((FMOD_VECTOR){new_sound->pos[0], new_sound->pos[1], 0}), &((FMOD_VECTOR){0}));
		}

		FMOD_Channel_SetPosition(sound_channels[i], new_sound->offset, FMOD_TIMEUNIT_MS);
		FMOD_Channel_SetPaused(sound_channels[i], FALSE);
	}

	for (TrackSlots i = 0; i < (TrackSlots)TS_SIZE; i++) {
		const TrackObject* new_track = &as->tracks[i];
		TrackObject* old_track = &audio_state.tracks[i];
		if (old_track->track_key == new_track->track_key && old_track->flags == new_track->flags)
			continue;

		stop_state_track(i);

		const Track* new_asset = get_track_key(new_track->track_key);
		if (new_asset == NULL || (!(new_track->flags & PLAY_LOOPING) && new_track->offset >= new_asset->length))
			continue;

		FMOD_System_PlaySound(speaker, new_asset->stream, state_music_group, TRUE, &music_channels[i]);
		FMOD_Channel_SetMode(music_channels[i],
			((new_track->flags & PLAY_LOOPING) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
		FMOD_Channel_SetMute(music_channels[i], i < as->top_track);
		FMOD_Channel_SetPosition(music_channels[i], new_track->offset, FMOD_TIMEUNIT_MS);
		FMOD_Channel_SetPaused(music_channels[i], FALSE);

		if (i > as->top_track)
			for (TrackSlots j = 0; j < i; j++)
				if (music_channels[j] != NULL)
					FMOD_Channel_SetMute(music_channels[j], TRUE);
	}

	SDL_memcpy(&audio_state, as, sizeof(audio_state));
}

void nuke_audio_state() {
	FMOD_ChannelGroup_Stop(state_sound_group);
	FMOD_ChannelGroup_Stop(state_music_group);
	SDL_memset(&audio_state, 0, sizeof(audio_state));
	SDL_memset(&sound_channels, 0, sizeof(sound_channels));
	SDL_memset(&music_channels, 0, sizeof(music_channels));
}

void pause_audio_state(Bool pause) {
	FMOD_ChannelGroup_SetPaused(state_sound_group, pause);
	FMOD_ChannelGroup_SetPaused(state_music_group, pause);
}

void play_state_sound(const char* name, PlayFlags flags, Uint32 offset, const float pos[2]) {
	const StTinyKey key = StHashStr(name);
	const Sound* sound = get_sound_key(key);
	if (sound == NULL) {
		WARN("Unknown sound \"%s\"", name);
		return;
	}
	if (!(flags & PLAY_LOOPING) && offset >= sound->length)
		return;

	const size_t idx = audio_state.next_sound;
	SoundObject* sobj = &audio_state.sounds[idx];
	sobj->sound_key = key;
	sobj->flags = flags;
	sobj->offset = offset;
	sobj->pos[0] = pos[0], sobj->pos[0] = pos[1];

	if (sound_channels[idx] != NULL)
		FMOD_Channel_Stop(sound_channels[idx]);
	FMOD_System_PlaySound(speaker, sound->sound, state_sound_group, TRUE, &sound_channels[idx]);
	FMOD_Channel_SetPosition(sound_channels[idx], offset, FMOD_TIMEUNIT_MS);

	if (flags & PLAY_PAN) {
		FMOD_Channel_SetPan(sound_channels[idx], pos[0]);
	} else if (flags & PLAY_POS) {
		FMOD_Channel_SetMode(sound_channels[idx], FMOD_3D | FMOD_3D_LINEARROLLOFF);
		FMOD_Channel_Set3DMinMaxDistance(sound_channels[idx], HALF_SCREEN_WIDTH, SCREEN_WIDTH);
		FMOD_Channel_Set3DAttributes(
			sound_channels[idx], &((FMOD_VECTOR){pos[0], pos[1], 0}), &((FMOD_VECTOR){0}));
	}

	FMOD_Channel_SetPaused(sound_channels[idx], FALSE);

	audio_state.next_sound = (idx + 1) % MAX_SOUNDS;
}

void play_state_track(TrackSlots slot, const char* name, PlayFlags flags, Uint32 offset) {
	const StTinyKey key = StHashStr(name);
	const Track* track = get_track_key(key);
	if (track == NULL) {
		WARN("Unknown track \"%s\"", name);
		return;
	}
	if (!(flags & PLAY_LOOPING) && offset >= track->length)
		return;

	TrackObject* tobj = &audio_state.tracks[slot];
	tobj->track_key = key;
	tobj->offset = offset;
	tobj->flags = flags;

	if (music_channels[slot] != NULL)
		FMOD_Channel_Stop(music_channels[slot]);
	FMOD_System_PlaySound(speaker, track->stream, state_music_group, TRUE, &music_channels[slot]);
	FMOD_Channel_SetMode(
		music_channels[slot], ((flags & PLAY_LOOPING) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
	FMOD_Channel_SetPosition(music_channels[slot], offset, FMOD_TIMEUNIT_MS);
	FMOD_Channel_SetMute(music_channels[slot], slot < audio_state.top_track);
	FMOD_Channel_SetPaused(music_channels[slot], FALSE);

	if (slot > audio_state.top_track) {
		for (TrackSlots i = 0; i < slot; i++)
			if (music_channels[i] != NULL)
				FMOD_Channel_SetMute(music_channels[i], TRUE);
		audio_state.top_track = slot;
	}
}

static void stop_state_sound(size_t idx) {
	SoundObject* sound = &audio_state.sounds[idx];
	sound->sound_key = 0;

	if (sound_channels[idx] != NULL) {
		FMOD_Channel_Stop(sound_channels[idx]);
		sound_channels[idx] = NULL;
	}
}

void stop_state_track(TrackSlots slot) {
	TrackObject* track = &audio_state.tracks[slot];
	track->track_key = 0;

	if (music_channels[slot] != NULL) {
		FMOD_Channel_Stop(music_channels[slot]);
		music_channels[slot] = NULL;
	}

	if (audio_state.top_track == slot && slot > 0) {
		int i = slot - 1;
		while (i >= 0) {
			if (music_channels[i] != NULL) {
				FMOD_Channel_SetMute(music_channels[i], FALSE);
				audio_state.top_track = i;
				break;
			}
			--i;
		}
	}
}

void move_ears(const float pos[2]) {
	FMOD_System_Set3DListenerAttributes(speaker, 0, &((FMOD_VECTOR){pos[0], pos[1], -HALF_SCREEN_WIDTH}),
		&((FMOD_VECTOR){0}), &((FMOD_VECTOR){0, 0, -1}), &((FMOD_VECTOR){0, -1, 0}));
}
