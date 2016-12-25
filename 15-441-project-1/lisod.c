#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pool.h"
#include "utils.h"
#include "log.h"

#define BUF_SIZE 4096

struct {
    int http_port;
    const char* log_filename;
    const char* www_folder; 
} options;

struct pool_t pool;

const char* mimetype_lookup[5][2] = {{"html", "text/html"},
                                     {"css", "text/css"}, 
                                     {"png", "image/png"}, 
                                     {"jpg", "image/jpeg"}, 
                                     {"gif", "image/gif"}};

const char* get_mimetype(const char* ext) {
    for (int i = 0; i != 5; ++i) {
        if (!strcasecmp(ext, mimetype_lookup[i][0])) {
            return mimetype_lookup[i][1];
        }
    }
    return "application/octet-stream";
}

int rbsend(int sockfd, char* b, int len) {
    int sendret = 0;
    while (len > 0) {
        sendret = send(sockfd, b, len, 0);
        if (sendret <= 0) {
            return 0;
        }
        len -= sendret;
    } 
    return 1;
}

int send_response(struct conn_t* conn, Response* response) {
    int sockfd = conn->sockfd;
    int success = 1;
    char sbuf[BUF_SIZE];
    sprintf(sbuf, "%s %d %s\r\n", response->http_version, response->status_code, response->reason_phrase);
    success = success && rbsend(sockfd, sbuf, strlen(sbuf));
    Response_header* p = response->headers;
    while (success && p != NULL) {
        sprintf(sbuf, "%s: %s\r\n", p->header_name, p->header_value);
        success = success && rbsend(sockfd, sbuf, strlen(sbuf));
        p = p->next;
    }
    success = success && rbsend(sockfd, "\r\n", 2);
    return success;
}

int send_file(struct conn_t* conn, FILE* file, int size) {
    char buf[BUF_SIZE];
    int nread = 0;
    while (size > 0) {
        nread = min(fread(buf, 1, BUF_SIZE, file), size);
        if (nread <= 0 || !rbsend(conn->sockfd, buf, nread)) {
            return 0;
        }
        size -= nread;
    }
    return 1;
}

void send_error(struct conn_t* conn, StatusCode status_code) {
    Response response;
    response_init(&response, status_code);
    response_add_header(&response, "Content-Type", "text/html");     
    response_add_header(&response, "Connection", "close");
    send_response(conn, &response);
    response_destroy(&response);
    pool_remove_conn(&pool, conn); 
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
    printf("%s\n", path);
    FILE* file = fopen(path, "r");
    if (file == NULL || stat(path, &statbuf) != 0) {
        send_error(conn, NOT_FOUND);
        return;
    }
    int close_conn = 0;
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
    close_conn = close_conn || !send_response(conn, &response);
    response_destroy(&response);
    close_conn = close_conn || !send_file(conn, file, content_length);
    if (close_conn) {
        pool_remove_conn(&pool, conn);
    }
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
    int close_conn = 0;
    Response response;
    response_init(&response, OK);
    close_conn = close_conn || !send_response(conn, &response);
    response_destroy(&response);
    if (close_conn) {
        pool_remove_conn(&pool, conn);
    }
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
    int close_conn = 0;
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
    close_conn = close_conn || !send_response(conn, &response);
    response_destroy(&response);
    if (close_conn) {
        pool_remove_conn(&pool, conn);
    }
}

int parse_options(int argc, char* argv[]) {
    if (argc != 9) {
        return 0;
    } 
    options.http_port = atoi(argv[1]);
    options.log_filename = argv[3];
    options.www_folder = argv[5];
    return 1;
}

int main(int argc, char* argv[])
{
    if (!parse_options(argc, argv)) {
        fprintf(stdout, "usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder> <CGI script path> <private key file> <certificate file>\n");
        return 1;
    }
    fprintf(stdout, "----- Lisod Server -----\n");
    pool_init(&pool, options.http_port, FD_SETSIZE);
    if (!pool_start(&pool)) {
        fprintf(stderr, "Failed to initialize connection pool\n");
        return 1;
    }
    log_init(LOG_DEBUG, options.log_filename);
    struct conn_t* conn;
    int sockfd;

    char buf[BUF_SIZE];

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        conn = pool_next_conn(&pool);
        sockfd = conn->sockfd;
        int recvret;
        recvret = recv(sockfd, buf, BUF_SIZE, 0);
        if (recvret >= 1) {
            int buf_offset = 0;
            Request* request;
            while (buf_offset != recvret) {
                request = parse(&(conn->parser), buf, &buf_offset, recvret);
                if (request != NULL) {
                    log(LOG_INFO, "handle request: %s %s %s\n", request->http_method, request->http_uri, request->http_version);
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
                }
            }
        } else {
            pool_remove_conn(&pool, conn);
        }
    }
    log_cleanup();
    return 0;
}
