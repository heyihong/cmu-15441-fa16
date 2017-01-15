#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "cgi.h"
#include "utils.h"
#include "log.h"

static char* new_env(char* key, char * value) {
    if (value == NULL) {
        return key;
    }
    char* p = malloc(strlen(key) + strlen(value) + 1);
    strcpy(p, key);
    strcat(p, value);
    return p;
}

void cgi_init(Cgi* cgi, char* script_path, 
    Request* request, struct in_addr addr, int server_port) {
    cgi->infd = cgi->outfd = -1;
    cgi->request = request;
    cgi->req_content_length = 0;
    cgi->state = request->content_length == 0 ? CGI_SEND : CGI_RECV;
    int stdin_pipe[2];
    int stdout_pipe[2];
    if (pipe(stdin_pipe) < 0) {
        cgi->state = CGI_FAILED;
        log_(LOG_ERROR, "Error piping for stdin.\n");
        return;
    }
    if (pipe(stdout_pipe) < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        cgi->state = CGI_FAILED;
        log_(LOG_ERROR, "Error piping for stdout.\n");
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        dup2(stdout_pipe[1], fileno(stdout));
        dup2(stdin_pipe[0], fileno(stdin));
        char* argv[] = {script_path, NULL};
        char* port_str;
        asprintf(&port_str, "%d", server_port);
        char* envp[] = {
                    new_env("CONTENT_LENGTH=", request_get_header(request, "Content-Length")),
                    new_env("CONTENT-TYPE=", request_get_header(request, "Content-Type")),
                    "GATEWAY_INTERFACE=CGI/1.1",
                    new_env("PATH_INFO=", request->abs_path + 4), // skip /cgi
                    new_env("QUERY_STRING=", request->query),
                    new_env("REMOTE_ADDR=", inet_ntoa(addr)),
                    new_env("REQUEST_METHOD=", request->http_method),
                    "SCRIPT_NAME=/w/api.php",
                    new_env("SERVER_PORT=", port_str),
                    "SERVER_PROTOCOL=HTTP/1.1",
                    "SERVER_SOFTWARE=Liso/1.0",
                    new_env("HTTP_ACCEPT=", request_get_header(request, "Accept")),
                    new_env("HTTP_REFERER=", request_get_header(request, "Referer")),
                    new_env("HTTP_ACCEPT_ENCODING=", request_get_header(request, "Accept-Encoding")),
                    new_env("HTTP_ACCEPT_LANGUAGE=", request_get_header(request, "Accept-Language")),
                    new_env("HTTP_ACCEPT_CHARSET=", request_get_header(request, "Accept-Charset")),
                    new_env("HTTP_COOKIE=", request_get_header(request, "Cookie")),
                    new_env("HTTP_USER_AGENT=", request_get_header(request, "User-Agent")),
                    new_env("HTTP_CONNECTION=", request_get_header(request, "Connection")),
                    new_env("HTTP_HOST=", request_get_header(request, "Host")),
                    NULL
               };
        // successful execve doesn't return
        execve(script_path, argv, envp);
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);
        cgi->infd = stdin_pipe[1];
        cgi->outfd = stdout_pipe[0];
        log_(LOG_DEBUG, "Create a cgi process, pid = %d, script_path = %s\n", pid, script_path);
    }
}

void cgi_read(Cgi* cgi, Buffer* buf) {
    if (cgi->state != CGI_SEND) {
        log_(LOG_WARN, "cgi_read is called when cig state isn't CGI_SEND\n");
        return;
    }
    int readret = read(cgi->outfd, buffer_input_ptr(buf), buffer_input_size(buf));
    if (readret < 0) {
        cgi->state = CGI_FAILED;
    } else if (readret == 0) {
        cgi->state = CGI_FINISHED;
    } else {
        buffer_input(buf, readret);
    }
}

void cgi_write(Cgi* cgi, Buffer* buf) {
    if (cgi->state != CGI_RECV) {
        log_(LOG_WARN, "cgi_write is called when cig state isn't CGI_RECV\n");
        return;
    }
    int len = min(buffer_output_size(buf), cgi->request->content_length - cgi->req_content_length);
    int writeret = write(cgi->infd, buffer_output_ptr(buf), len);
    if (writeret < 0) {
        cgi->state = CGI_FAILED; 
        return;
    }
    buffer_output(buf, writeret);
    cgi->req_content_length += len;
    if (cgi->req_content_length == cgi->request->content_length) {
        cgi->state = CGI_SEND;
    }
}

void cgi_destroy(Cgi* cgi) {
    if (cgi->infd >= 0) {
        close(cgi->infd);
    }
    if (cgi->outfd >= 0) {
        close(cgi->outfd);
    }
    request_destroy(cgi->request);
    free(cgi->request);
    log_(LOG_DEBUG, "Cancel the cgi process\n");
}
