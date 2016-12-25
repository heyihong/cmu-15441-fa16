#ifndef __POOL_H__
#define __POOL_H__

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "parse.h"

struct conn_t {
	int sockfd;
	Parser parser;
	struct conn_t* prev;
	struct conn_t* next;
};

struct pool_t {
	int port;
	int max_conns;
	int num_conns;
	struct conn_t* conns; 
	struct conn_t* conn_iter;
	
	int ac_sock;
	fd_set readfs;
};

void conn_init(struct conn_t* conn, int sockfd);

void conn_destroy(struct conn_t* conn);

void pool_init(struct pool_t* pool, int ac_sock, int max_conns);

void pool_destroy(struct pool_t* pool);

int pool_start(struct pool_t* pool);

int pool_add_conn(struct pool_t* pool, int sockfd);

void pool_remove_conn(struct pool_t* pool, struct conn_t* conn);

struct conn_t* pool_next_conn(struct pool_t* pool);

#endif