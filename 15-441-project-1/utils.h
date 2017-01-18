#ifndef __UTILS_H__
#define __UTILS_H__

#include <time.h>

int min(int x, int y);

int max(int x, int y);

int is_valid_port(int port);

char* new_str(const char* str);

char* new_strn(const char* str, int n);

const char *get_filename_ext(const char *filename);

const char* get_mimetype(const char* ext);

void get_http_format_date(time_t* time, char* str, int size);

void enable_non_blocking(int fd);

#endif