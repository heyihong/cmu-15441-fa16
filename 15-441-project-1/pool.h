#ifndef __POOL_H__
#define __POOL_H__

#include <openssl/ssl.h>

#include "conn.h"

struct Pool {
	int max_conns;
	int num_conns;
	Conn* conns; 
	SSL_CTX* ssl_context;	

	int http_sock;
	int https_sock;
    fd_set writefs;
	fd_set readfs;
};

typedef struct Pool Pool;

void pool_init(Pool* pool, int max_conns);

void pool_destroy(Pool* pool);

void pool_start(Pool* pool, int http_port, int https_port, const char* key_file, const char* crt_file);

int pool_is_full(Pool* pool);

void pool_add_conn(Pool* pool, Conn* new_conn);

void pool_remove_conn(Pool* pool, Conn* conn);

void pool_wait_io(Pool* pool);

Conn* pool_next_conn(Pool* pool);

#endif