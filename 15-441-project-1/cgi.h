#ifndef __CGI_H__
#define __CGI_H__

#include <netinet/ip.h>

#include "http.h"
#include "buffer.h"

enum CgiState {
    CGI_RECV,
    CGI_SEND,
    CGI_FAILED,
    CGI_FINISHED
};

typedef enum CgiState CgiState;

struct Cgi {
    int infd;
    int outfd;
    Request* request;
    int req_content_length;
    CgiState state;
};

typedef struct Cgi Cgi;

void cgi_init(Cgi* cgi, char* script_path, 
    Request* request, struct in_addr addr, int server_port);

void cgi_read(Cgi* cgi, Buffer* buf);

void cgi_write(Cgi* cgi, Buffer* buf);

void cgi_destroy(Cgi* cgi);

#endif