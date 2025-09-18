#include "K_memory.h"

// https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
static const StTinyKey FNV_OFFSET = 0x811c9dc5, FNV_PRIME = 0x01000193;
StTinyKey long_key(const char* str) {
    StTinyKey key = FNV_OFFSET;
    for (const char* c = str; *c; c++) {
        key ^= (StTinyKey)(unsigned char)(*c);
        key *= FNV_PRIME;
    }
    return key;
}
