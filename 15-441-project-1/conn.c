#include "conn.h"
#include "io.h"
#include "log.h"

#include <unistd.h>
#include <errno.h>

void conn_init(Conn *conn, int sockfd, SSL *ssl, struct in_addr addr) {
    conn->sockfd = sockfd;
    conn->addr = addr;
    conn->ssl = ssl;
    conn->cgi = NULL;
    conn->handle = NULL;
    buffer_init(&(conn->in_buf));
    buffer_init(&(conn->out_buf));
    parser_init(&(conn->parser));
    conn->state = RECV_REQ_HEAD;
    conn->prev = conn->next = NULL;
}

int conn_send(Conn *conn) {
    Buffer *buf = &(conn->out_buf);
    if (conn->state == CONN_CLOSE || buffer_is_empty(buf)) {
        return 1;
    }
    if (io_wait_read(conn->sockfd) || io_wait_write(conn->sockfd)) {
        return 0;
    }
    int ret;
    if (conn->ssl != NULL) {
        ret = SSL_write(conn->ssl, buffer_output_ptr(buf), buffer_output_size(buf));
        if (ret <= 0) {
            switch (SSL_get_error(conn->ssl, ret)) {
                case SSL_ERROR_WANT_READ:
                    io_need_read(conn->sockfd);
                    return 0;
                case SSL_ERROR_WANT_WRITE:
                    io_need_write(conn->sockfd);
                    return 0;
                default:
                    conn->state = CONN_CLOSE;
                    return 1;
            }
        }
    } else {
        ret = send(conn->sockfd, buffer_output_ptr(buf), buffer_output_size(buf), 0);
        if (ret < 0) {
            switch (errno) {
                case EAGAIN:
                    io_need_write(conn->sockfd);
                    return 0;
                default:
                    conn->state = CONN_CLOSE;
                    return 1;
            }
        }
    }
    buffer_output(buf, ret);
    log_(LOG_DEBUG, "Connection send %d byte(s)\n", ret);
    return 1;
}

int conn_recv(Conn *conn) {
    Buffer *buf = &(conn->in_buf);
    if (conn->state == CONN_CLOSE || buffer_is_full(buf)) {
        return 1;
    }
    if (io_wait_read(conn->sockfd) || io_wait_write(conn->sockfd)) {
        return 0;
    }
    int ret;
    if (conn->ssl != NULL) {
        ret = SSL_read(conn->ssl, buffer_input_ptr(buf), buffer_input_size(buf));
        if (ret <= 0) {
            switch (SSL_get_error(conn->ssl, ret)) {
                case SSL_ERROR_WANT_READ:
                    io_need_read(conn->sockfd);
                    return 0;
                case SSL_ERROR_WANT_WRITE:
                    io_need_write(conn->sockfd);
                    return 0;
                default:
                    conn->state = CONN_CLOSE;
                    return 1;
            }
        }
    } else {
        ret = recv(conn->sockfd, buffer_input_ptr(buf), buffer_input_size(buf), 0);
        if (ret < 0) {
            switch (errno) {
                case EAGAIN:
                    io_need_read(conn->sockfd);
                    return 0;
                default:
                    conn->state = CONN_CLOSE;
                    return 1;
            }
        } else if (ret == 0) {
            conn->state = CONN_CLOSE;
            return 1;
        }
    }
    buffer_input(buf, ret);
    log_(LOG_DEBUG, "Connection receive %d byte(s)\n", ret);
    return 1;
}

void conn_destroy(Conn *conn) {
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
