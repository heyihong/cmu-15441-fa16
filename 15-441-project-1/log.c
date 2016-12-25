#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>

#include "log.h"

static FILE* log_file;

static LogLevel log_level;

void log_init(LogLevel lv, const char* filename) {
	log_level = lv;
	log_file = fopen(filename, "w");
	assert(log_file != NULL);
}

void log(LogLevel lv, const char* fmt, ...) {
	if (lv < log_level) {
		return;
	}
	time_t now = time(0);
	char str[4096];	
	struct tm tm = *gmtime(&now);
  	strftime(str, sizeof(str), "%Y %x %H:%M:%S", &tm);
	fprintf(log_file, "%s ", str);
	switch (lv) {
		case LOG_DEBUG:
			fprintf(log_file, "DEBUG ");
			break;
		case LOG_INFO:
			fprintf(log_file, "INFO ");
			break;
		case LOG_ERROR:
			fprintf(log_file, "ERROR ");
			break;
	}
    va_list arg;
    va_start(arg, fmt);
    vfprintf(log_file, fmt, arg);
    va_end(arg);
   	fflush(log_file); 
}

void log_cleanup() {
	fclose(log_file);
}