#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include "pool.h"
#include "utils.h"
#include "log.h"

#define BUF_SIZE 4096

struct {
    int http_port;
    int https_port;
    char* lock_file;
    char* log_file;
    char* www_folder; 
    char* cgi_script;
    char* key_file;
    char* crt_file;
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

void handle_conn(Conn* conn, fd_set* readfs, fd_set* writefs) {
    int readable = FD_ISSET(conn->sockfd, readfs);
    int writable = FD_ISSET(conn->sockfd, writefs);
    int cgi_readable = conn->cgi != NULL && FD_ISSET(conn->cgi->outfd, readfs);
    int cgi_writable = conn->cgi != NULL && FD_ISSET(conn->cgi->infd, writefs);
    log_(LOG_DEBUG, "Handling connection: sockfd = %d, readable = %d, writable = %d, cgi_readable = %d, cgi_writable = %d\n", 
        conn->sockfd, readable > 0, writable > 0, cgi_readable > 0, cgi_writable > 0);
    while (1) {
        switch (conn->state) {
            case RECV_REQ_HEAD: {
                int need_conn_recv = !buffer_is_full(&(conn->buf));
                Request* request = parser_parse(&(conn->parser), &(conn->buf));
                if (request != NULL) {
                    log_(LOG_INFO, "handle request: %s %s %s\n", request->http_method, request->abs_path, request->http_version);
                    if (strstr(request->abs_path, "/cgi/") == request->abs_path) {
                        conn->cgi = (Cgi*)malloc(sizeof(Cgi));
                        cgi_init(conn->cgi, options.cgi_script, request, conn->addr, 
                            conn->ssl == NULL ? options.http_port : options.https_port);
                        conn->state = CGI_RECV_REQ_BODY;
                    } else {
                        conn->handle = (Handle*)malloc(sizeof(Handle));
                        handle_init(conn->handle, options.www_folder, request);
                        conn->state = RECV_REQ_BODY;
                    }
                } else if (conn->parser.state == STATE_FAILED) {
                    conn->state = CONN_FAILED;
                } else if (need_conn_recv && readable) {
                    conn_recv(conn);
                    readable = 0;
                } else {
                    if (need_conn_recv) {
                        pool_add_readfd(&pool, conn->sockfd);
                    }                    
                    return;
                }
            }
            break;
            case RECV_REQ_BODY: {
                int need_conn_recv = !buffer_is_full(&(conn->buf));
                if (conn->handle->state != HANDLE_RECV) {
                    // discard remain bytes in buffer
                    buffer_init(&(conn->buf));
                    conn->state = SEND_RES;
                } else if (!buffer_is_empty(&(conn->buf))) {
                    handle_write(conn->handle, &(conn->buf));
                } else if (need_conn_recv && readable) {
                    conn_recv(conn);
                    readable = 0;
                } else {
                    if (need_conn_recv) {
                        pool_add_readfd(&pool, conn->sockfd);
                    }
                    return;
                }
            }
            break; 
            case SEND_RES: { 
                int need_conn_send = !buffer_is_empty(&(conn->buf));
                if (conn->handle->state == HANDLE_FAILED) {
                    conn->state = CONN_FAILED;
                } else if (!buffer_is_full(&(conn->buf)) 
                    && (conn->handle->state == HANDLE_PROCESS || conn->handle->state == HANDLE_SEND)) {
                    handle_read(conn->handle, &(conn->buf));
                } else if (need_conn_send && writable) {
                    conn_send(conn);
                    writable = 0; 
                } else if (buffer_is_empty(&(conn->buf)) && conn->handle->state == HANDLE_FINISHED) {
                    conn->state = CONN_CLOSE; 
                } else {
                    if (need_conn_send) {
                        pool_add_writefd(&pool, conn->sockfd);
                    }
                    return;
                }
            }
            break;
            case CGI_RECV_REQ_BODY: {
                int need_cgi_write = !buffer_is_empty(&(conn->buf));
                int need_conn_recv = !buffer_is_full(&(conn->buf));
                if (conn->cgi->state != CGI_RECV) {
                    // discard remain bytes in buffer
                    buffer_init(&(conn->buf));
                    conn->state = CGI_SEND_RES;
                } else if (need_cgi_write && cgi_writable) {
                    cgi_write(conn->cgi, &(conn->buf));
                    cgi_writable = 0;
                } else if (need_conn_recv && readable) {
                    conn_recv(conn);
                    readable = 0;
                } else {
                    if (need_cgi_write) {
                        pool_add_writefd(&pool, conn->cgi->infd);
                    }
                    if (need_conn_recv) {
                        pool_add_readfd(&pool, conn->sockfd); 
                    }
                    return;
                }
            }
            break;
            case CGI_SEND_RES: {
                int need_cgi_read = !buffer_is_full(&(conn->buf)) && conn->cgi->state == CGI_SEND;
                int need_conn_send = !buffer_is_empty(&(conn->buf));
                if (conn->cgi->state == CGI_FAILED) {
                    conn->state = CONN_FAILED;
                } else if (need_cgi_read && cgi_readable) {
                    cgi_read(conn->cgi, &(conn->buf));
                    cgi_readable = 0;
                } else if (need_conn_send && writable) {
                    conn_send(conn);
                    writable = 0; 
                } else if (buffer_is_empty(&(conn->buf)) && conn->cgi->state == CGI_FINISHED) {
                    conn->state = CONN_CLOSE; 
                } else {
                    if (need_conn_send) {
                        pool_add_writefd(&pool, conn->sockfd);
                    }
                    if (need_cgi_read) {
                        pool_add_readfd(&pool, conn->cgi->outfd); 
                    }
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
        fd_set readfs = pool.readfs;
        fd_set writefs = pool.writefs;
        pool_clear_fs(&pool);
        Conn* p = pool.conns;
        while (p != NULL) {
            Conn* conn = p;
            p = p->next;
            // conn may be removed
            handle_conn(conn, &readfs, &writefs);
        }
        pool_wait_io(&pool);
    }

    return EXIT_SUCCESS;
}
