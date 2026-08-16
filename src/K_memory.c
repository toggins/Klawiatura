#include <SDL3/SDL_endian.h>

#include "K_memory.h"
#include "K_misc.h"

Buffer buffer_from(void* data, size_t size) {
    return (Buffer){
        .data = data,
        .cursor = data,
        .size = size,
    };
}

size_t buffer_tell(const Buffer buffer) {
    return buffer.cursor - buffer.data;
}

#define READ_BUFFERX(bits, swap)                                                                                       \
    void read_buffer##bits(Buffer* buffer, Uint##bits* dest) {                                                         \
        if ((buffer_tell(*buffer) + sizeof(Uint##bits)) > buffer->size)                                                \
            return;                                                                                                    \
                                                                                                                       \
        *dest = swap(*(Uint##bits*)buffer->cursor);                                                                    \
        buffer->cursor += sizeof(Uint##bits);                                                                          \
    }

READ_BUFFERX(8, (Uint8));
READ_BUFFERX(16, SDL_Swap16LE);
READ_BUFFERX(32, SDL_Swap32LE);
READ_BUFFERX(64, SDL_Swap64LE);

#undef READ_BUFFERX

void read_buffer_string(Buffer* buffer, char* dest, size_t size) {
    size_t i = 0;
    while (TRUE) {
        char c = '\0';
        read_buffer8(buffer, (Uint8*)&c);

        if (i < size)
            dest[i++] = c;
        if (c == '\0') {
            dest[size - 1] = '\0';
            break;
        }
    }
}

#define WRITE_BUFFERX(bits, swap)                                                                                      \
    void write_buffer##bits(Buffer* buffer, const Uint##bits* src) {                                                   \
        if ((buffer_tell(*buffer) + sizeof(Uint##bits)) > buffer->size)                                                \
            return;                                                                                                    \
                                                                                                                       \
        *(Uint##bits*)buffer->cursor = swap(*src);                                                                     \
        buffer->cursor += sizeof(Uint##bits);                                                                          \
    }

WRITE_BUFFERX(8, (Uint8));
WRITE_BUFFERX(16, SDL_Swap16LE);
WRITE_BUFFERX(32, SDL_Swap32LE);
WRITE_BUFFERX(64, SDL_Swap64LE);

#undef WRITE_BUFFERX

void write_buffer_string(Buffer* buffer, const char* dest, size_t size) {
    Buffer sbuf = buffer_from((void*)dest, size);
    while (TRUE) {
        char c = '\0';
        read_buffer8(&sbuf, (Uint8*)&c);

        write_buffer8(buffer, (Uint8*)&c);
        if (c == '\0')
            break;
    }
}
