#pragma once

#include "K_misc.h"

#define BASE32_STRING_SIZE 14 // max length is 13 + null terminator

const char *fmt(const char*, ...), *vfmt(const char*, va_list);
const char* caret(Bool);

const char* u64_to_base32(Uint64);
Uint64 base32_to_u64(const char*);
