#include "pool.h"

int pool_init(struct pool_t* pool, int port, int max_conns) {
  pool->max_conns = max_conns;
  pool->num_conns = 0;
  pool->conns = NULL;
  pool->conn_iter = NULL;
  struct sockaddr_in addr;

  /* all networked programs must create a socket */
  if ((pool->ac_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Failed creating socket.\n");
    return 0;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
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

void pool_destroy(struct pool_t* pool) {
  while (pool->conns != NULL) {
    pool_remove_conn(pool, pool->conns);
  }
  close(pool->ac_sock);
}

int pool_add_conn(struct pool_t* pool, int sockfd) {
    if (pool->num_conns == pool->max_conns) {
        return 0;
    }
    pool->num_conns++;
    struct conn_t* new_conn = malloc(sizeof(struct conn_t));
    new_conn->sockfd = sockfd;
    new_conn->prev = NULL;
    new_conn->next = pool->conns;
    if (pool->conns != NULL) {
      pool->conns->prev = new_conn;
    }
    pool->conns = new_conn;
    return 1;
}

void pool_remove_conn(struct pool_t* pool, struct conn_t* conn) {
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
      fprintf(stdout, "add sockfd %d to conn\n", sockfd);
      if (!pool_add_conn(pool, sockfd)) {
        fprintf(stdout, "failed to add sockfd %d to the full pool\n", sockfd);
        close(sockfd);
      }
    }
    pool->conn_iter = pool->conns;
  }
  return p;
}