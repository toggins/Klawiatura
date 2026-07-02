#pragma once

#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_iostream.h>

#include <yyjson.h>

#include "K_misc.h"

#define JSON_READ_FLAGS (YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS)
#define JSON_WRITE_FLAGS (YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END)

void file_init(const char**), file_teardown();
void load_mod(const char*);

yyjson_doc* read_json(const char*, size_t);

SDL_IOStream* stream_base_file(const char*);

void* load_data_file(const char*, size_t*);
SDL_IOStream* stream_data_file(const char*, const char*);
yyjson_doc* load_data_json(const char*);
void iterate_data_files(const char*, Bool, void (*)(const char*, const void*, size_t, void*), void*);

void* load_user_file(const char*, size_t*);
yyjson_doc* load_user_json(const char*);
void iterate_user_files(const char*, Bool, void (*)(const char*, const void*, size_t, void*), void*);
Bool save_user_file(const char*, void*, size_t), save_user_folder(const char*);

const char *file_basename(const char*), *filename_no_ext(const char*);

void read_float_le(SDL_IOStream*, float*);
void read_string(SDL_IOStream*, char*, size_t);
