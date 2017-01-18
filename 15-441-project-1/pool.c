#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "io.h"
#include "utils.h"
#include "pool.h"
#include "log.h"

static void pool_http_start(Pool *pool, int http_port) {
    struct sockaddr_in addr;

    /* all networked programs must create a socket */
    if ((pool->http_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        log_(LOG_ERROR, "Failed creating socket.\n");
        exit(EXIT_FAILURE);
    }
    int yes = 1;
    if (setsockopt(pool->http_sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    enable_non_blocking(pool->http_sock);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(http_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(pool->http_sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close(pool->http_sock);
        log_(LOG_ERROR, "Failed binding socket at port %d.\n", http_port);
        exit(EXIT_FAILURE);
    }

    if (listen(pool->http_sock, 5)) {
        close(pool->http_sock);
        log_(LOG_ERROR, "Error listening on socket.\n");
        exit(EXIT_FAILURE);
    }
}

static void pool_https_start(Pool *pool,
                             int https_port,
                             const char *key_file,
                             const char *crt_file) {
    struct sockaddr_in addr;
    /************ SSL INIT ************/
    SSL_load_error_strings();
    SSL_library_init();

    /* we want to use TLSv1 only */
    if ((pool->ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL) {
        log_(LOG_ERROR, "Error creating SSL context.\n");
        exit(EXIT_FAILURE);
    }

    /* register private key */
    if (SSL_CTX_use_PrivateKey_file(pool->ssl_context, key_file,
                                    SSL_FILETYPE_PEM) == 0) {
        SSL_CTX_free(pool->ssl_context);
        log_(LOG_ERROR, "Error associating private key.\n");
        exit(EXIT_FAILURE);
    }

    /* register public key (certificate) */
    if (SSL_CTX_use_certificate_file(pool->ssl_context, crt_file,
                                     SSL_FILETYPE_PEM) == 0) {
        SSL_CTX_free(pool->ssl_context);
        log_(LOG_ERROR, "Error associating certificate.\n");
        exit(EXIT_FAILURE);
    }
    /************ END SSL INIT ************/

    /************ SERVER SOCKET SETUP ************/
    if ((pool->https_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        SSL_CTX_free(pool->ssl_context);
        log_(LOG_ERROR, "Failed creating socket.\n");
        exit(EXIT_FAILURE);
    }
    int yes = 1;
    if (setsockopt(pool->https_sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    enable_non_blocking(pool->https_sock);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(https_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(pool->https_sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close(pool->https_sock);
        SSL_CTX_free(pool->ssl_context);
        log_(LOG_ERROR, "Failed binding socket at port %d.\n", https_port);
        exit(EXIT_FAILURE);
    }

    if (listen(pool->https_sock, 5)) {
        close(pool->https_sock);
        SSL_CTX_free(pool->ssl_context);
        log_(LOG_ERROR, "Error listening on socket.\n");
        exit(EXIT_FAILURE);
    }
    /************ END SERVER SOCKET SETUP ************/
}

void pool_init(Pool *pool, int max_conns) {
    pool->max_conns = max_conns;
    pool->num_conns = 0;
    pool->conns = NULL;
    pool->ssl_context = NULL;
}

void pool_start(Pool *pool, int http_port, int https_port, const char *key_file, const char *crt_file) {
    pool_http_start(pool, http_port);
    pool_https_start(pool, https_port, key_file, crt_file);
}

void pool_destroy(Pool *pool) {
    while (pool->conns != NULL) {
        pool_remove_conn(pool, pool->conns);
    }
    close(pool->http_sock);
    close(pool->https_sock);
    SSL_CTX_free(pool->ssl_context);
}

int pool_is_full(Pool *pool) {
    return pool->num_conns == pool->max_conns;
}

void pool_add_conn(Pool *pool, Conn *new_conn) {
    enable_non_blocking(new_conn->sockfd);
    log_(LOG_DEBUG, "add new connection, sockfd = %d\n", new_conn->sockfd);
    pool->num_conns++;
    new_conn->next = pool->conns;
    if (pool->conns != NULL) {
        pool->conns->prev = new_conn;
    }
    pool->conns = new_conn;
}

void pool_remove_conn(Pool *pool, Conn *conn) {
    log_(LOG_DEBUG, "remove connection, sockfd = %d\n", conn->sockfd);
    pool->num_conns--;
    if (conn->prev != NULL) {
        conn->prev->next = conn->next;
    }
    if (conn->next != NULL) {
        conn->next->prev = conn->prev;
    }
    if (pool->conns == conn) {
        pool->conns = conn->next;
    }
    conn_destroy(conn);
    free(conn);
}

void pool_wait_io(Pool *pool) {
    log_(LOG_DEBUG, "The connection pool is waiting for io\n");
    io_need_read(pool->http_sock);
    io_need_read(pool->https_sock);
    io_wait();
    int sockfd;
    struct sockaddr_in cli_addr;
    socklen_t cli_size;
    while (1) {
        // add a new http connection to the pool if it has
        cli_size = sizeof(cli_addr);
        sockfd = accept(pool->http_sock, (struct sockaddr *) &cli_addr, &cli_size);
        if (sockfd < 0) {
            switch (errno) {
                case EAGAIN:
                    break;
                default:
                    perror("accept");
            }
            break;
        } else if (!pool_is_full(pool)) {
            Conn *conn = (Conn *) malloc(sizeof(Conn));
            conn_init(conn, sockfd, NULL, cli_addr.sin_addr);
            pool_add_conn(pool, conn);
        } else {
            close(sockfd);
            log_(LOG_INFO,
                 "Drop the new http connection due to the connection pool is full, number of connections = %d\n",
                 pool->num_conns);
        }
    }
    while (1) {
        // add a new https connection to the pool if it has
        cli_size = sizeof(cli_addr);
        sockfd = accept(pool->https_sock, (struct sockaddr *) &cli_addr, &cli_size);
        if (sockfd < 0) {
            switch (errno) {
                case EAGAIN:
                    break;
                default:
                    perror("accept");
            }
            break;
        } else if (!pool_is_full(pool)) {
            SSL *ssl = SSL_new(pool->ssl_context);
            if (ssl != NULL) {
                int success = 1;
                if (SSL_set_fd(ssl, sockfd) == 0) {
                    log_(LOG_ERROR, "Error creating client SSLcontext .\n");
                    success = 0;
                } else {
                    SSL_set_accept_state(ssl);
                }
                if (success) {
                    Conn *conn = (Conn *) malloc(sizeof(Conn));
                    conn_init(conn, sockfd, ssl, cli_addr.sin_addr);
                    pool_add_conn(pool, conn);
                } else {
                    SSL_free(ssl);
                    close(sockfd);
                }
            } else {
                close(sockfd);
                log_(LOG_ERROR, "Error creating client SSL context.\n");
            }
        } else {
            close(sockfd);
            log_(LOG_INFO,
                 "Drop the new https connection due to the connection pool is full, number of connections = %d\n",
                 pool->num_conns);
        }
    }
}
