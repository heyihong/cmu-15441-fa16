#ifndef __UTILS_H__
#define __UTILS_H__

#include <time.h>

int min(int x, int y);

const char *get_filename_ext(const char *filename);

void get_http_format_date(time_t* time, char* str, int size);

#endif