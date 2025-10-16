#include <stdlib.h>
// ^ required for `exit(EXIT_FAILURE)` below. DO NOT TOUCH YOU FUCKER

#include "K_file.h"
#include "K_log.h"

const char* log_basename(const char* path) {
	return file_basename(path);
}

void DIE() {
	exit(EXIT_FAILURE);
}
