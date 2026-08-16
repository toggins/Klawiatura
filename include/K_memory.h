#pragma once

#include <SDL3/SDL_stdinc.h>

#define S_TRUCTURES_NOSTD
#include <S_tructures.h>

typedef struct {
    const size_t size;
    Uint8 *data, *cursor;
} Buffer;

Buffer buffer_from(void*, size_t);
size_t buffer_tell(const Buffer);
void read_buffer8(Buffer*, Uint8*), read_buffer16(Buffer*, Uint16*), read_buffer32(Buffer*, Uint32*),
    read_buffer64(Buffer*, Uint64*), read_buffer_string(Buffer*, char*, size_t);
void write_buffer8(Buffer*, const Uint8*), write_buffer16(Buffer*, const Uint16*),
    write_buffer32(Buffer*, const Uint32*), write_buffer64(Buffer*, const Uint64*),
    write_buffer_string(Buffer*, const char*, size_t);
