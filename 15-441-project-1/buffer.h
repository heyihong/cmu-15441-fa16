#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "http.h"

#define BUFFER_MAX_SIZE 4096

struct Buffer {
    char data[BUFFER_MAX_SIZE];
    int begin;
    int end;
};

typedef struct Buffer Buffer;

void buffer_init(Buffer* buf);

void buffer_init_by_response(Buffer* buf, Response* response);

char* buffer_input_ptr(Buffer* buf);

int buffer_input_size(Buffer* buf);

char* buffer_output_ptr(Buffer* buf);

int buffer_output_size(Buffer* buf);

void buffer_input(Buffer* buf, int size);

void buffer_output(Buffer* buf, int size);

int buffer_is_empty(Buffer* buf);

int buffer_is_full(Buffer* buf);

#endif