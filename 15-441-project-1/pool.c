#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"
#include "pool.h"
#include "log.h"

static void pool_http_start(struct pool_t* pool, int http_port) { 
  struct sockaddr_in addr;

  /* all networked programs must create a socket */
  if ((pool->http_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    log_(LOG_ERROR, "Failed creating socket.\n");
    exit(EXIT_FAILURE);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(http_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  /* servers bind sockets to ports---notify the OS they accept connections */
  if (bind(pool->http_sock, (struct sockaddr *) &addr, sizeof(addr))) {
    close(pool->http_sock);
    log_(LOG_ERROR, "Failed binding socket at port %d.\n", http_port);
    exit(EXIT_FAILURE);
  }


  if (listen(pool->http_sock, 5)){
    close(pool->http_sock);
    log_(LOG_ERROR, "Error listening on socket.\n");
    exit(EXIT_FAILURE);
  }
}

static void pool_https_start(struct pool_t* pool,
                             int https_port,
                             const char* key_file,
                             const char* crt_file) {
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
                                   SSL_FILETYPE_PEM) == 0)
  {
    SSL_CTX_free(pool->ssl_context);
    log_(LOG_ERROR, "Error associating certificate.\n");
    exit(EXIT_FAILURE);
  }
  /************ END SSL INIT ************/

  /************ SERVER SOCKET SETUP ************/
  if ((pool->https_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
  {
      SSL_CTX_free(pool->ssl_context);
      log_(LOG_ERROR, "Failed creating socket.\n");
      exit(EXIT_FAILURE);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(https_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(pool->https_sock, (struct sockaddr *) &addr, sizeof(addr)))
  {
      close(pool->https_sock);
      SSL_CTX_free(pool->ssl_context);
      log_(LOG_ERROR, "Failed binding socket at port %d.\n", https_port);
      exit(EXIT_FAILURE);
  }

  if (listen(pool->https_sock, 5))
  {
      close(pool->https_sock);
      SSL_CTX_free(pool->ssl_context);
      log_(LOG_ERROR, "Error listening on socket.\n");
      exit(EXIT_FAILURE);
  }
  /************ END SERVER SOCKET SETUP ************/
}

void conn_init(struct conn_t* conn, int sockfd, SSL* ssl) {
  conn->sockfd = sockfd;
  conn->ssl = ssl;
  conn->failed = conn->close = 0;
  parser_init(&(conn->parser));
  conn->prev = conn->next = NULL;
}

void conn_send(struct conn_t* conn, char* b, int len) {
  if (conn->failed) {
    return;
  }
  int ret = 1;
  if (conn->ssl != NULL) {
    SSL* ssl = conn->ssl;
    while (len > 0 && ret > 0) {
      ret = SSL_write(ssl, b, len);
      if (ret > 0) {
        b += ret; len -= ret;
      } else {
        break;
      }
    }
  } else {
    int sockfd = conn->sockfd;
    while (len > 0) {
      ret = send(sockfd, b, len, 0);
      if (ret > 0) {
        b += ret; len -= ret;
      } else {
        break;
      }
    } 
  }
  if (ret <= 0) {
    conn->failed = 1;
  }
}

int conn_recv(struct conn_t* conn, char* b, int len) {
  if (conn->failed) {
    return 0;
  }
  int ret;
  if (conn->ssl != NULL) {
    ret = SSL_read(conn->ssl, b, len);
  } else {
    ret = read(conn->sockfd, b, len);
  }
  if (ret <= 0) {
    conn->failed = 1;
  }
  return ret;
}

void conn_destroy(struct conn_t* conn) {
  if (conn->ssl != NULL) {
    SSL_shutdown(conn->ssl);
    SSL_free(conn->ssl);
  }
  close(conn->sockfd);
  parser_destroy(&(conn->parser)); 
}

void pool_init(struct pool_t* pool, int max_conns) {
  pool->max_conns = max_conns;
  pool->num_conns = 0;
  pool->conns = NULL;
  pool->conn_iter = NULL;
  pool->ssl_context = NULL;
}

void pool_start(struct pool_t* pool, int http_port, int https_port, const char* key_file, const char* crt_file) {
   pool_http_start(pool, http_port);
   pool_https_start(pool, https_port, key_file, crt_file);
}

void pool_destroy(struct pool_t* pool) {
  while (pool->conns != NULL) {
    pool_remove_conn(pool, pool->conns);
  }
  close(pool->http_sock);
  close(pool->https_sock);
  SSL_CTX_free(pool->ssl_context);
}

int pool_is_full(struct pool_t* pool) {
  return pool->num_conns == pool->max_conns;
}

void pool_add_conn(struct pool_t* pool, struct conn_t* new_conn) {
    log_(LOG_DEBUG, "add new connection, sockfd = %d\n", new_conn->sockfd);
    pool->num_conns++;
    new_conn->next = pool->conns;
    if (pool->conns != NULL) {
      pool->conns->prev = new_conn;
    }
    pool->conns = new_conn;
}

void pool_remove_conn(struct pool_t* pool, struct conn_t* conn) {
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

struct conn_t* pool_next_conn(struct pool_t* pool) {
  struct conn_t* p = NULL;
  while (1) {
    p = pool->conn_iter;
    while (p != NULL) {
      if (FD_ISSET(p->sockfd, &(pool->readfs))) {
        break;
      }
      p = p->next;
    }
    // find next available connection
    if (p != NULL) {
      pool->conn_iter = p->next; 
      break;
    }
    // use select to find available connections
    int max_fd = pool->http_sock;
    FD_ZERO(&(pool->readfs));
    FD_SET(pool->http_sock, &(pool->readfs));
    FD_SET(pool->https_sock, &(pool->readfs));
    p = pool->conns;
    while (p != NULL) {
      if (max_fd < p->sockfd) {
        max_fd = p->sockfd;
      }
      FD_SET(p->sockfd, &(pool->readfs));
      p = p->next;
    }
    int rv = select(max_fd + 1, &(pool->readfs), NULL, NULL, NULL);
    if (rv == -1) {
        perror("select");
    } 
    // add a new http connection to the pool if it has
    if (FD_ISSET(pool->http_sock, &(pool->readfs))) {
      struct sockaddr_in cli_addr;
      socklen_t cli_size = sizeof(cli_addr);
      int sockfd = accept(pool->http_sock, (struct sockaddr *) &cli_addr, &cli_size);
      if (sockfd == -1) {
        perror("accept");
      }
      if (!pool_is_full(pool)) {
        struct conn_t* conn = (struct conn_t*)malloc(sizeof(struct conn_t));
        conn_init(conn, sockfd, NULL);
        pool_add_conn(pool, conn);
      } else {
        close(sockfd);
        log_(LOG_INFO, "Drop the new http connection due to the connection pool is full, number of connections = %d\n", pool->num_conns);
      }
    }
    // add a new https connection to the pool if it has
    if (FD_ISSET(pool->https_sock, &(pool->readfs))) {
      struct sockaddr_in cli_addr;
      socklen_t cli_size = sizeof(cli_addr);
      int sockfd = accept(pool->https_sock, (struct sockaddr *) &cli_addr, &cli_size);
      if (sockfd == -1) {
        perror("accept");
      }
      if (!pool_is_full(pool)) {
        SSL* ssl = SSL_new(pool->ssl_context);
        if (ssl != NULL) {
          int success = 1;
          if (SSL_set_fd(ssl, sockfd) == 0) {
            log_(LOG_ERROR, "Error creating client SSL context.\n");
            success = 0;
          } else if (SSL_accept(ssl) <= 0) {
            log_(LOG_ERROR, "Error accepting (handshake) client SSL context.\n");
            success = 0;
          }
          if (success) {
            struct conn_t* conn = (struct conn_t*)malloc(sizeof(struct conn_t));
            conn_init(conn, sockfd, ssl);
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
        log_(LOG_INFO, "Drop the new https connection due to the connection pool is full, number of connections = %d\n", pool->num_conns);
      }
    }
    pool->conn_iter = pool->conns;
  }
  return p;
}