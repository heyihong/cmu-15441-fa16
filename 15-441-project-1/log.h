#ifndef __LOG_H__
#define __LOG_H__

enum LogLevel {
	LOG_DEBUG = 0,
	LOG_INFO = 1,
	LOG_ERROR = 2 
};

typedef enum LogLevel LogLevel;

void log_init(LogLevel lv, const char* filename);

void log(LogLevel lv, const char* fmt, ...);

void log_cleanup();

#endif