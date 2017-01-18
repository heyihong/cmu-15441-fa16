#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "handle.h"
#include "utils.h"
#include "log.h"

static char tmpbuf[4096];

static int check_http_version(Request* request) {
    return strcmp(request->http_version, "HTTP/1.1") == 0;
}

static Response* do_get(Handle* handle) { 
    if (!check_http_version(handle->request)) {
        return response_error(HTTP_VERSION_NOT_SUPPORTED);
    }
    char* path = tmpbuf;
    strcpy(path, handle->www_folder);
    strcat(path, "/");
    strcat(path, handle->request->abs_path);
    struct stat statbuf;
    // the path is a directory
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        strcat(path, "/index.html");
    }
    FILE* file = fopen(path, "r");
    if (file == NULL || stat(path, &statbuf) != 0) {
        return response_error(NOT_FOUND);
    }
    int content_length = statbuf.st_size;
    handle->file = file;
    handle->res_content_length = content_length;
    Response* response = (Response*)malloc(sizeof(Response));
    response_init(response, OK);
    response_add_header(response, "Content-Type", get_mimetype(get_filename_ext(path)));
    char* str = tmpbuf;
    sprintf(str, "%d", content_length);
    response_add_header(response, "Content-Length", str);
    get_http_format_date(&(statbuf.st_ctime), str, sizeof(tmpbuf));
    response_add_header(response, "Last-Modified", str);
    return response;
}

static Response* do_post(Handle* handle) {
    if (!check_http_version(handle->request)) {
        return response_error(HTTP_VERSION_NOT_SUPPORTED);
    }
    Response* response = (Response*)malloc(sizeof(Response));
    response_init(response, OK);
    return response;
}

static Response* do_head(Handle* handle) {
    if (!check_http_version(handle->request)) {
        return response_error(HTTP_VERSION_NOT_SUPPORTED);
    }
    char* path = tmpbuf;
    strcpy(path, handle->www_folder);
    strcat(path, "/");
    strcat(path, handle->request->abs_path);
    struct stat statbuf;
    // the path is a directory
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        strcat(path, "/index.html");
    }
    if (stat(path, &statbuf) != 0) {
        return response_error(NOT_FOUND);
    }
    int content_length = statbuf.st_size;
    Response* response = (Response*)malloc(sizeof(Response));
    response_init(response, OK);
    response_add_header(response, "Content-Type", get_mimetype(get_filename_ext(path)));
    char* str = tmpbuf;
    sprintf(str, "%d", content_length);
    response_add_header(response, "Content-Length", str);
    get_http_format_date(&(statbuf.st_ctime), str, sizeof(tmpbuf));
    response_add_header(response, "Last-Modified", str);
    return response;
}

void handle_init(Handle* handle, char* www_folder, Request* request) {
    handle->www_folder = www_folder;
    handle->request = request;
    handle->req_content_length = 0;
    handle->res_content_length = 0;
    handle->last_req = 0;
    handle->file = NULL;
    handle->state = request->content_length <= 0 ? HANDLE_PROCESS : HANDLE_RECV; 
}

void handle_read(Handle* handle, Buffer* buf) {
    while (1) {
        switch (handle->state) {
            case HANDLE_PROCESS: {
                Request* request = handle->request;
                // always close connection according to project document
                Response* response;
                if (handle->request->content_length < 0) {
                    response = response_error(REQUEST_ENTITY_TOO_LARGE);
                } else if (!strcmp(request->http_method, "GET")) {
                    response = do_get(handle);
                } else if (!strcmp(request->http_method, "POST")) {
                    response = do_post(handle);
                } else if (!strcmp(request->http_method, "HEAD")) {
                    response = do_head(handle);
                } else {
                    response = response_error(NOT_IMPLEMENTED);
                }
                // handle connection token
                if (response->status_code != OK || request_connection_close(handle->request)) {
                    response_add_header(response, "Connection", "close");
                    handle->last_req = 1;
                }
                // assume the buffer can hold the whole response
                buffer_init_by_response(buf, response);
                buf->data[buf->end] = 0;
                log_(LOG_DEBUG, "Response\n%s", buf->data);
                response_destroy(response);
                free(response); 
                if (handle->file != NULL) {
                    handle->state = HANDLE_SEND;
                } else {
                    handle->state = HANDLE_FINISHED;
                    return;
                }
            }
            break;
            case HANDLE_SEND: {
                int nread = 0;
                while (buffer_input_size(buf) > 0 && handle->res_content_length > 0) {
                    nread = min(fread(buffer_input_ptr(buf), 1, buffer_input_size(buf), handle->file)
                               ,handle->res_content_length);
                    if (nread <= 0) {
                        handle->state = HANDLE_FAILED;
                        return;
                    }
                    handle->res_content_length -= nread;
                    buffer_input(buf, nread);
                }
                if (handle->res_content_length == 0) {
                    handle->state = HANDLE_FINISHED;
                }
                return;
            }
            default: {
                log_(LOG_WARN, "handle_read is called when handle state isn't HANDLE_PROCESS or HANDLE_SEND\n");
                return;
            }
        }
    }
}

void handle_write(Handle* handle, Buffer* buf) { 
    if (handle->state != HANDLE_RECV) {
        log_(LOG_WARN, "handle_read is called when handle state isn't HANDLE_RECV\n");
        return;
    }
    int len = min(buffer_output_size(buf), handle->request->content_length - handle->req_content_length);
    buffer_output(buf, len);
    handle->req_content_length += len;
    if (handle->req_content_length == handle->request->content_length) {
        handle->state = HANDLE_PROCESS;
    }
}

void handle_destroy(Handle* handle) {
    request_destroy(handle->request);
    free(handle->request);
    if (handle->file != NULL) {
        fclose(handle->file);
    } 
}