#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "pool.h"
#include "utils.h"
#include "log.h"

#define BUF_SIZE 4096

struct {
    int http_port;
    int https_port;
    const char* lock_file;
    const char* log_file;
    const char* www_folder; 
    const char* cgi_script;
    const char* key_file;
    const char* crt_file;
} options;

Pool pool;

void lisod_shutdown(int exit_stat) {
    pool_destroy(&pool); 
    log_cleanup();
    exit(exit_stat);
}

void signal_handler(int sig)
{
    switch(sig)
    {
        case SIGHUP:
            /* rehash the server */
            break;          
        case SIGTERM:
            /* finalize and shutdown the server */
            lisod_shutdown(EXIT_SUCCESS);
            break;    
        default:
            break;
            /* unhandled signal */      
    }       
}

int daemonize(const char* lock_file)
{
    /* drop to having init() as parent */
    int i, lfp, pid = fork();
    char str[256] = {0};
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    setsid();

    for (i = getdtablesize(); i>=0; i--)
            close(i);

    i = open("/dev/null", O_RDWR);
    dup(i); /* stdout */
    dup(i); /* stderr */
    umask(027);

    lfp = open(lock_file, O_RDWR|O_CREAT, 0640);
    
    if (lfp < 0)
            exit(EXIT_FAILURE); /* can not open */

    if (lockf(lfp, F_TLOCK, 0) < 0)
            exit(EXIT_SUCCESS); /* can not lock */
    
    /* only first instance continues */
    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); /* record pid to lockfile */

    signal(SIGCHLD, SIG_IGN); /* child terminate signal */

    signal(SIGHUP, signal_handler); /* hangup signal */
    signal(SIGTERM, signal_handler); /* software termination signal from kill */

    return EXIT_SUCCESS;
}

int parse_options(int argc, char* argv[]) {
    if (argc != 9) {
        return 0;
    } 
    options.http_port = atoi(argv[1]);
    options.https_port = atoi(argv[2]);
    options.log_file = argv[3];
    options.lock_file = argv[4];
    options.www_folder = argv[5];
    options.cgi_script = argv[6];
    options.key_file = argv[7];
    options.crt_file = argv[8];
    return is_valid_port(options.http_port) && is_valid_port(options.https_port);
}

void handle_conn(Conn* conn) {
    int readable = FD_ISSET(conn->sockfd, &(pool.readfs));
    int writable = FD_ISSET(conn->sockfd, &(pool.writefs));
    log_(LOG_DEBUG, "Connection %d readable = %d, writable = %d\n", conn->sockfd, readable > 0, writable > 0);
    // int cgi_readable = conn->cgi != NULL && FD_ISSET(conn->cgi->outfd, &(pool.readfs));
    // int cgi_writeable = conn->cgi != NULL && FD_ISSET(conn->cgi->infd, &(pool.writefs));
    while (1) {
        switch (conn->state) {
            case RECV_REQ_HEAD: {
                Request* request = parser_parse(&(conn->parser), &(conn->buf));
                if (request != NULL) {
                    if (strstr(request->abs_path, "/cig/") == request->abs_path) {
                        conn->state = CGI_RECV_REQ_BODY;
                    } else {
                        conn->handle = (Handle*)malloc(sizeof(Handle));
                        handle_init(conn->handle, options.www_folder, request);
                        conn->state = RECV_REQ_BODY;
                    }
                } else if (conn->parser.state == STATE_FAILED) {
                    conn->state = CONN_FAILED;
                } else if (readable) {
                    conn_recv(conn);
                    readable = 0;
                } else {
                    return;
                }
            }
            break;
            case RECV_REQ_BODY: {
                if (conn->handle->request->content_length == 0) {
                    conn->state = SEND_RES;
                } else if (!buffer_is_empty(&(conn->buf))) {
                    handle_write(conn->handle, &(conn->buf));
                } else if (readable) {
                    conn_recv(conn);
                    readable = 0;
                } else {
                    return;
                }
            }
            break; 
            case SEND_RES: { 
                handle_read(conn->handle, &(conn->buf));
                if (conn->handle->state == HANDLE_FAILED) {
                    conn->state = CONN_FAILED;
                } else if (writable) {
                    conn_send(conn);
                    writable = 0; 
                    if (buffer_is_empty(&(conn->buf)) && conn->handle->state == HANDLE_FINISHED) {
                        conn->state = CONN_CLOSE; 
                    } 
                } else {
                    return;
                }
            }
            break;
            case CONN_CLOSE: {
                pool_remove_conn(&pool, conn);
                return;
            }
            case CONN_FAILED: {
                pool_remove_conn(&pool, conn);
                return;
            }
            break;
            case CGI_RECV_REQ_BODY: case CGI_SEND_RES: {
                log_(LOG_ERROR, "CGI not implemented\n");
                conn->state = CONN_CLOSE;
            }
            break;
        }
    } 
}

int main(int argc, char* argv[])
{
    if (!parse_options(argc, argv)) {
        fprintf(stdout, "usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder> <CGI script path> <private key file> <certificate file>\n");
        exit(EXIT_FAILURE);
    }
    // if (daemonize(options.lock_file) == EXIT_FAILURE) {
    //     exit(EXIT_FAILURE);
    // }
    fprintf(stdout, "----- Lisod Server -----\n");
    log_init(LOG_DEBUG, options.log_file);
    pool_init(&pool, FD_SETSIZE);
    pool_start(&pool, options.http_port, options.https_port, options.key_file, options.crt_file);
    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        pool_wait_io(&pool);
        Conn* p = pool.conns;
        while (p != NULL) {
            Conn* conn = p;
            p = p->next;
            // conn may be removed
            handle_conn(conn);
        }
    }

    return EXIT_SUCCESS;
}
