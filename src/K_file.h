#pragma once

#include <yyjson.h>

void file_init(const char*);
void file_teardown();

const char* file_pattern(const char*, ...);
const char* find_data_file(const char*, const char*);
const char* find_user_file(const char*, const char*);
