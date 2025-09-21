#pragma once

#include <yyjson.h>

#define JSON_READ_FLAGS (YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS)

void file_init(const char*);
void file_teardown();

const char* file_pattern(const char*, ...);
const char* find_data_file(const char*, const char*);
const char* find_user_file(const char*, const char*);
