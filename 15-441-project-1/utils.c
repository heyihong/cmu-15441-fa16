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


const char* mimetype_lookup[5][2] = {{"html", "text/html"},
                                     {"css", "text/css"}, 
                                     {"png", "image/png"}, 
                                     {"jpg", "image/jpeg"}, 
                                     {"gif", "image/gif"}};

const char* get_mimetype(const char* ext) {
    int i;
    for (i = 0; i != 5; ++i) {
        if (!strcasecmp(ext, mimetype_lookup[i][0])) {
            return mimetype_lookup[i][1];
        }
    }
    return "application/octet-stream";
}