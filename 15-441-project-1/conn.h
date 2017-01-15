#ifndef __CONN_H__
#define __CONN_H__

#include <openssl/ssl.h>
#include <netinet/in.h>

#include "buffer.h"
#include "parse.h"
#include "cgi.h"
#include "handle.h"

enum ConnState {
    RECV_REQ_HEAD,
    RECV_REQ_BODY,
    SEND_RES,
    CGI_RECV_REQ_BODY,
    CGI_SEND_RES,
    CONN_FAILED,
    CONN_CLOSE
};

typedef enum ConnState ConnState;

struct Conn {
    int sockfd;
    struct in_addr addr;
    SSL* ssl;
    Handle* handle;
    Cgi* cgi;
    Buffer buf;
    Parser parser;
    ConnState state; 
    struct Conn* prev;
    struct Conn* next;
};

typedef struct Conn Conn;

void conn_init(Conn* conn, int sockfd, SSL* ssl, struct in_addr addr);

void conn_send(Conn* conn);

void conn_recv(Conn* conn);

void conn_destroy(Conn* conn);

#endif