#include <string.h>

#include "utils.h"

int min(int x, int y) {
	return x < y ? x : y;
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void get_http_format_date(time_t* now, char* str, int size) {
	struct tm tm = *gmtime(now);
  	strftime(str, size, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}