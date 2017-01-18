#ifndef __HANDLE_H__
#define __HANDLE_H__

#include <stdio.h>

#include "http.h"
#include "buffer.h"

enum HandleState {
    HANDLE_RECV,
    HANDLE_PROCESS,
    HANDLE_SEND,
    HANDLE_FAILED,
    HANDLE_FINISHED
};

typedef enum HandleState HandleState;

struct Handle {
    const char* www_folder;
    Request* request;
    int req_content_length;
    int res_content_length;
    int last_req;
    FILE* file;
    HandleState state;
};

typedef struct Handle Handle;

void handle_init(Handle* handle, char* www_foler, Request* request);

void handle_read(Handle* handle, Buffer* buf);

void handle_write(Handle* handle, Buffer* buf);

void handle_destroy(Handle* handle);

#endif