#ifndef __CGI_H__
#define __CGI_H__

#include "http.h"
#include "buffer.h"

struct Cgi {
    int pid;
    int infd;
    int outfd;
};

typedef struct Cgi Cgi;

void cgi_init(Cgi* cgi, const char* script_path, Request* request);

void cgi_read(Cgi* cgi, Buffer* buf);

void cgi_write(Cgi* cgi, Buffer* buf);

void cgi_destroy(Cgi* cgi);

#endif