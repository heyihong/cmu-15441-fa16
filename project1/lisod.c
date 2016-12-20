#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "pool.h"

#define ECHO_PORT 10001
#define BUF_SIZE 4096


int main(int argc, char* argv[])
{
    struct pool_t pool;

    fprintf(stdout, "----- Lisod Server -----\n");

    if (!pool_init(&pool, ECHO_PORT, FD_SETSIZE)) {
        fprintf(stderr, "Failed to initialize connection pool");
    }

    struct conn_t* conn;
    int sockfd;
    char buf[BUF_SIZE];

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        conn = pool_next_conn(&pool);
        sockfd = conn->sockfd;
        int recvret;
        int sendret;
        int sendlen;
        recvret = recv(sockfd, buf, BUF_SIZE, 0);
        fprintf(stdout, "receive from sockfd %d, size = %d\n", sockfd, recvret);
        if (recvret >= 1) {
            sendlen = 0;
            while (sendlen != recvret && (sendret = send(sockfd, buf + sendlen, recvret - sendlen, 0)) > 0) {
                sendlen += sendret;
            }
        }
        if (recvret <= 0 || sendret <= 0) {
            pool_remove_conn(&pool, conn);
            fprintf(stdout, "remove sockfd %d from conn\n", sockfd);
        }
    }

    pool_destroy(&pool);

    return EXIT_SUCCESS;
}
