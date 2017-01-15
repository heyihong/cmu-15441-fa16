#include "conn.h"

#include <sys/socket.h>
#include <unistd.h>

void conn_init(Conn* conn, int sockfd, SSL* ssl, struct in_addr addr) {
  conn->sockfd = sockfd;
  conn->addr = addr;
  conn->ssl = ssl;
  conn->cgi = NULL;
  conn->handle = NULL;
  parser_init(&(conn->parser));
  buffer_init(&(conn->buf));
  conn->state = RECV_REQ_HEAD;
  conn->prev = conn->next = NULL;
}

void conn_send(Conn* conn) {
  Buffer* buf = &(conn->buf);
  if (conn->state == CONN_FAILED || buffer_is_empty(buf)) {
    return;
  }
  int ret;
  if (conn->ssl != NULL) {
    SSL* ssl = conn->ssl;
    ret = SSL_write(ssl, buffer_output_ptr(buf), buffer_output_size(buf));
  } else {
    ret = send(conn->sockfd, buffer_output_ptr(buf), buffer_output_size(buf), 0);
  }
  if (ret < 0) {
    conn->state = CONN_FAILED;
  } else {
    buffer_output(buf, ret);
  }
}

void conn_recv(Conn* conn) {
  Buffer* buf = &(conn->buf);
  if (conn->state == CONN_FAILED || buffer_is_full(buf)) {
    return;
  }
  int ret;
  if (conn->ssl != NULL) {
    ret = SSL_read(conn->ssl, buffer_input_ptr(buf), buffer_input_size(buf));
  } else {
    ret = recv(conn->sockfd, buffer_input_ptr(buf), buffer_input_size(buf), 0);
  }
  if (ret < 0) {
    conn->state = CONN_FAILED;
  } else if (ret == 0) {
    conn->state = CONN_CLOSE;
  } else {
    buffer_input(buf, ret);
  }
}

void conn_destroy(Conn* conn) {
  if (conn->ssl != NULL) {
    SSL_shutdown(conn->ssl);
    SSL_free(conn->ssl);
  }
  if (conn->handle != NULL) {
    handle_destroy(conn->handle);
    free(conn->handle);
  }
  if (conn->cgi != NULL) {
    cgi_destroy(conn->cgi);
    free(conn->cgi);
  }
  close(conn->sockfd);
}
