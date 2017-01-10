#ifndef __POOL_H__
#define __POOL_H__

#include <openssl/ssl.h>

#include "parse.h"

struct conn_t {
	int sockfd;
	// use SSL if ssl != NULL	
    SSL *ssl;
	int failed;
	int close;
	Parser parser;
	struct conn_t* prev;
	struct conn_t* next;
};

struct pool_t {
	int max_conns;
	int num_conns;
	struct conn_t* conns; 
	struct conn_t* conn_iter;
	SSL_CTX* ssl_context;	

	int http_sock;
	int https_sock;
	fd_set readfs;
};

void conn_init(struct conn_t* conn, int sockfd, SSL* ssl);

void conn_send(struct conn_t* conn, char*b, int len);

int conn_recv(struct conn_t* conn, char*b, int len);

void conn_destroy(struct conn_t* conn);

void pool_init(struct pool_t* pool, int max_conns);

void pool_destroy(struct pool_t* pool);

void pool_start(struct pool_t* pool, int http_port, int https_port, const char* key_file, const char* crt_file);

int pool_is_full(struct pool_t* pool);

void pool_add_conn(struct pool_t* pool, struct conn_t* new_conn);

void pool_remove_conn(struct pool_t* pool, struct conn_t* conn);

struct conn_t* pool_next_conn(struct pool_t* pool);

#endif