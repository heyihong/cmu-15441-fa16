/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define ECHO_PORT 10001
#define BUF_SIZE 4096

struct SocketConn {
    int sockfd;
    struct SocketConn* prev;
    struct SocketConn* next;
};

void add_socket_conn(struct SocketConn** conns, int sockfd) {
    struct SocketConn* new_conn = malloc(sizeof(struct SocketConn));
    new_conn->sockfd = sockfd;
    new_conn->prev = NULL;
    new_conn->next = *conns;
    if (*conns != NULL) {
      (*conns)->prev = new_conn;
    }
    *conns = new_conn;
} 

void remove_socket_conn(struct SocketConn** conns, struct SocketConn* conn) {
  if (conn->prev != NULL) {
    conn->prev->next = conn->next;
  }
  if (conn->next != NULL) {
    conn->next->prev = conn->prev;
  }
  if (*conns == conn) {
    *conns = conn->next;
  }
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int sock;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

    fprintf(stdout, "----- Lisod Server -----\n");
    
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    struct SocketConn* conns = NULL;
    fd_set readfs;
    int max_sock;
    int sockfd;
    struct SocketConn* p;
    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        max_sock = sock;
        p = conns;
        while (p != NULL) {
            if (max_sock < p->sockfd) {
                max_sock = p->sockfd;
            }
            fprintf(stdout, "add sockfd %d to set\n", p->sockfd);
            FD_SET(p->sockfd, &readfs);
            p = p->next;
        }
        int rv = select(max_sock + 1, &readfs, NULL, NULL, NULL);
        if (rv == -1) {
            perror("select");
        } else {
            if (FD_ISSET(sock, &readfs)) {
                cli_size = sizeof(cli_addr);
                sockfd = accept(sock, (struct sockaddr *) &cli_addr,
                                 &cli_size);
                if (sockfd == -1) {
                    perror("accept");
                } else {
                    fprintf(stdout, "add sockfd %d to conn\n", sockfd);
                    add_socket_conn(&conns, sockfd);
                }
            }
            int recvret;
            int sendret;
            int sendlen;
            struct SocketConn* conn_next;
            p = conns;
            while (p != NULL) {
                conn_next = p->next;
                sockfd = p->sockfd;
                if (FD_ISSET(sockfd, &readfs)) {
                    recvret = recv(sockfd, buf, BUF_SIZE, 0);
                    fprintf(stdout, "receive from sockfd %d, size = %d\n", sockfd, recvret);
                    if (recvret >= 1) {
                        sendlen = 0;
                        while (sendlen != recvret && (sendret = send(sockfd, buf + sendlen, recvret - sendlen, 0)) > 0) {
                            sendlen += sendret;
                        }
                    }
                    if (recvret <= 0 || sendret <= 0) {
                        close_socket(sockfd);
                        remove_socket_conn(&conns, p);
                        fprintf(stdout, "remove sockfd %d from conn\n", sockfd);
                    }
                }
                p = conn_next; 
            }
        }
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}
