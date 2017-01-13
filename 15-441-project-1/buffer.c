#include <stdio.h>
#include <string.h>

#include "buffer.h"

void buffer_init(Buffer* buf) {
    buf->begin = buf->end = 0;
}

void buffer_init_by_response(Buffer* buf, Response* response) {
    char* p = buf->data;
    *p = 0;
    sprintf(p, "%s %d %s\r\n", response->http_version, response->status_code, response->reason_phrase);
    p += strlen(p);
    ResponseHeader* header = response->headers;
    while (header != NULL) {
        sprintf(p, "%s: %s\r\n", header->header_name, header->header_value);
        p += strlen(p);
        header = header->next;
    }
    sprintf(p, "\r\n");
    buf->begin = 0;
    buf->end = strlen(buf->data);
}

char* buffer_input_ptr(Buffer* buf) {
    return buf->end < BUFFER_MAX_SIZE ? (buf->data + buf->end) : (buf->data + buf->end - BUFFER_MAX_SIZE);
}

int buffer_input_size(Buffer* buf) {
    return buf->end < BUFFER_MAX_SIZE ? (BUFFER_MAX_SIZE - buf->end) : (BUFFER_MAX_SIZE - (buf->end - buf->begin));
}


char* buffer_output_ptr(Buffer* buf) {
    return buf->data + buf->begin;
}

int buffer_output_size(Buffer* buf) {
    return buf->end < BUFFER_MAX_SIZE ? (buf->end - buf->begin) : (BUFFER_MAX_SIZE - buf->begin);
}

void buffer_input(Buffer* buf, int size) {
    buf->end += size;
}

void buffer_output(Buffer* buf, int size) {
    buf->begin += size;
    if (buf->begin == BUFFER_MAX_SIZE) {
        buf->begin = 0;
        buf->end -= BUFFER_MAX_SIZE;
    }
}

int buffer_is_empty(Buffer* buf) {
   return buf->begin == buf->end; 
}

int buffer_is_full(Buffer* buf) {
    return buf->end - buf->begin == BUFFER_MAX_SIZE;
}
