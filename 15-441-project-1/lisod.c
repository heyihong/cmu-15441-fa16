#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
    const char* log_filename;
    const char* www_folder; 
    const char* key_file;
    const char* crt_file;
} options;

struct pool_t pool;

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

void send_response(struct conn_t* conn, Response* response) {
    char sbuf[BUF_SIZE];
    sprintf(sbuf, "%s %d %s\r\n", response->http_version, response->status_code, response->reason_phrase);
    conn_send(conn, sbuf, strlen(sbuf));
    if (conn->close) {
        response_add_header(response, "Connection", "close");
    }
    Response_header* p = response->headers;
    while (!conn->failed && p != NULL) {
        sprintf(sbuf, "%s: %s\r\n", p->header_name, p->header_value);
        conn_send(conn, sbuf, strlen(sbuf));
        p = p->next;
    }
    conn_send(conn, "\r\n", 2);
}

void send_file(struct conn_t* conn, FILE* file, int size) {
    char buf[BUF_SIZE];
    int nread = 0;
    while (!conn->failed && size > 0) {
        nread = min(fread(buf, 1, BUF_SIZE, file), size);
        if (nread <= 0) {
            conn->failed = 1;
            return;
        }
        conn_send(conn, buf, nread);
        size -= nread;
    }
}

void send_error(struct conn_t* conn, StatusCode status_code) {
    Response response;
    response_init(&response, status_code);
    response_add_header(&response, "Content-Type", "text/html");     
    send_response(conn, &response);
    response_destroy(&response);
}

int check_http_version(struct conn_t* conn, Request* request) {
    if (strcmp(request->http_version, "HTTP/1.1")) {
        send_error(conn, HTTP_VERSION_NOT_SUPPORTED);
        return 0;
    }
    return 1;
}

void do_get(struct conn_t* conn, Request* request) { 
    if (!check_http_version(conn, request)) {
        return;
    }
    char path[BUF_SIZE];
    strcpy(path, options.www_folder);
    strcat(path, "/");
    strcat(path, request->http_uri);
    struct stat statbuf;
    // the path is a directory
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        strcat(path, "/index.html");
    }
    FILE* file = fopen(path, "r");
    if (file == NULL || stat(path, &statbuf) != 0) {
        send_error(conn, NOT_FOUND);
        return;
    }
    const char* ext = get_filename_ext(path);
    const char* mimetype = get_mimetype(ext);
    int content_length = statbuf.st_size;
    char str[BUF_SIZE];
    Response response;
    response_init(&response, OK);
    response_add_header(&response, "Content-Type", mimetype);
    sprintf(str, "%d", content_length);
    response_add_header(&response, "Content-Length", str);
    get_http_format_date(&(statbuf.st_ctime), str, BUF_SIZE);
    response_add_header(&response, "Last-Modified", str);
    send_response(conn, &response);
    response_destroy(&response);
    send_file(conn, file, content_length);
    fclose(file);
}

void do_post(struct conn_t* conn, Request* request) {
    if (!check_http_version(conn, request)) {
        return;
    }
    if (request_get_header(request, "Content-Length") == NULL) {
        send_error(conn, LENGTH_REQUIRED);
        return;
    }
    Response response;
    response_init(&response, OK);
    send_response(conn, &response);
    response_destroy(&response);
}

void do_head(struct conn_t* conn, Request* request) {
    if (!check_http_version(conn, request)) {
        return;
    }
    char path[BUF_SIZE];
    strcpy(path, options.www_folder);
    strcat(path, "/");
    strcat(path, request->http_uri);
    struct stat statbuf;
    // the path is a directory
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        strcat(path, "/index.html");
    }
    if (stat(path, &statbuf) != 0) {
        send_error(conn, NOT_FOUND);
        return;
    }
    const char* ext = get_filename_ext(path);
    const char* mimetype = get_mimetype(ext);
    int content_length = statbuf.st_size;
    char str[BUF_SIZE];
    Response response;
    response_init(&response, OK);
    response_add_header(&response, "Content-Type", mimetype);
    sprintf(str, "%d", content_length);
    response_add_header(&response, "Content-Length", str);
    get_http_format_date(&(statbuf.st_ctime), str, BUF_SIZE);
    response_add_header(&response, "Last-Modified", str);
    send_response(conn, &response);
    response_destroy(&response);
}

int parse_options(int argc, char* argv[]) {
    if (argc != 9) {
        return 0;
    } 
    options.http_port = atoi(argv[1]);
    options.https_port = atoi(argv[2]);
    options.log_filename = argv[3];
    options.lock_file = argv[4];
    options.www_folder = argv[5];
    options.key_file = argv[7];
    options.crt_file = argv[8];
    return 1;
}

int main(int argc, char* argv[])
{
    if (!parse_options(argc, argv)) {
        fprintf(stdout, "usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder> <CGI script path> <private key file> <certificate file>\n");
        exit(EXIT_FAILURE);
    }
    if (daemonize(options.lock_file) == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "----- Lisod Server -----\n");
    pool_init(&pool, FD_SETSIZE);
    pool_start(&pool, options.http_port, options.https_port, options.key_file, options.crt_file);
    log_init(LOG_DEBUG, options.log_filename);
    struct conn_t* conn;

    char buf[BUF_SIZE];

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        conn = pool_next_conn(&pool);
        int recvret;
        recvret = conn_recv(conn, buf, BUF_SIZE);
        int buf_offset = 0;
        Request* request;
        while (!conn->failed && !conn->close && buf_offset != recvret) {
            request = parse(&(conn->parser), buf, &buf_offset, recvret);
            if (request != NULL) {
                log_(LOG_INFO, "handle request: %s %s %s\n", request->http_method, request->http_uri, request->http_version);
                // always close connection according to project document
                conn->close = 1;
                if (!strcmp(request->http_method, "GET")) {
                    do_get(conn, request); 
                } else if (!strcmp(request->http_method, "POST")) {
                    do_post(conn, request);
                } else if (!strcmp(request->http_method, "HEAD")) {
                    do_head(conn, request);
                } else {
                    send_error(conn, NOT_IMPLEMENTED);
                }
                free(request);    
                break;
            }
        }
        if (conn->close || conn->failed) {
            pool_remove_conn(&pool, conn);
        }
    }
    return EXIT_SUCCESS;
}
