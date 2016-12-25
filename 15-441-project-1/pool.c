#include "utils.h"
#include "pool.h"
#include "log.h"

void conn_init(struct conn_t* conn, int sockfd) {
  conn->sockfd = sockfd;
  conn->failed = conn->close = 0;
  parser_init(&(conn->parser));
  conn->prev = conn->next = NULL;
}

void conn_destroy(struct conn_t* conn) {
  parser_destroy(&(conn->parser)); 
}

void pool_init(struct pool_t* pool, int port, int max_conns) {
  pool->port = port;
  pool->max_conns = max_conns;
  pool->num_conns = 0;
  pool->conns = NULL;
  pool->conn_iter = NULL;
}

void pool_destroy(struct pool_t* pool) {
  while (pool->conns != NULL) {
    pool_remove_conn(pool, pool->conns);
  }
  close(pool->ac_sock);
}

int pool_start(struct pool_t* pool) {
  struct sockaddr_in addr;

  /* all networked programs must create a socket */
  if ((pool->ac_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Failed creating socket.\n");
    return 0;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(pool->port);
  addr.sin_addr.s_addr = INADDR_ANY;

  /* servers bind sockets to ports---notify the OS they accept connections */
  if (bind(pool->ac_sock, (struct sockaddr *) &addr, sizeof(addr))) {
    close(pool->ac_sock);
    fprintf(stderr, "Failed binding socket.\n");
    return 0;
  }


  if (listen(pool->ac_sock, 5)){
    close(pool->ac_sock);
    fprintf(stderr, "Error listening on socket.\n");
    return 0;
  }
  return 1;
}

int pool_add_conn(struct pool_t* pool, int sockfd) {
    if (pool->num_conns == pool->max_conns) {
        return 0;
    }
    log(LOG_DEBUG, "add new connection, sockfd = %d\n", sockfd);
    pool->num_conns++;
    struct conn_t* new_conn = malloc(sizeof(struct conn_t));
    conn_init(new_conn, sockfd);
    new_conn->next = pool->conns;
    if (pool->conns != NULL) {
      pool->conns->prev = new_conn;
    }
    pool->conns = new_conn;
    return 1;
}

void pool_remove_conn(struct pool_t* pool, struct conn_t* conn) {
  log(LOG_DEBUG, "remove connection, sockfd = %d\n", conn->sockfd);
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
  close(conn->sockfd);
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
    int max_fd = pool->ac_sock;
    FD_ZERO(&(pool->readfs));
    FD_SET(pool->ac_sock, &(pool->readfs));
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
    // add new connection to the pool
    if (FD_ISSET(pool->ac_sock, &(pool->readfs))) {
      struct sockaddr_in cli_addr;
      socklen_t cli_size = sizeof(cli_addr);
      int sockfd = accept(pool->ac_sock, (struct sockaddr *) &cli_addr, &cli_size);
      if (sockfd == -1) {
        perror("accept");
      }
      if (!pool_add_conn(pool, sockfd)) {
        log(LOG_DEBUG, "failed to add sockfd %d to the full pool\n", sockfd);
        close(sockfd);
      }
    }
    pool->conn_iter = pool->conns;
  }
  return p;
}