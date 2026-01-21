#pragma once

#include <yyjson.h>

#define JSON_READ_FLAGS (YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS)
#define JSON_WRITE_FLAGS (YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END)

void file_init(const char*), file_teardown();
const char *find_data_file(const char*, const char*), *find_user_file(const char*, const char*);
const char *get_data_path(), *get_user_path();

const char *file_basename(const char*), *filename_sans_extension(const char*);
