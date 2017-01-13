#ifndef __PARSE_H__
#define __PARSE_H__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "http.h"

#define SUCCESS 0

enum ParserState {
	STATE_START = 0, 
    STATE_CR, 
    STATE_CRLF, 
    STATE_CRLFCR, 
    STATE_CRLFCRLF,
    STATE_FAILED
};

struct Parser {
	char buf[HTTP_HEADER_MAX_SIZE];
	int buf_len;
	enum ParserState state;
};

typedef struct Parser Parser;

void parser_init(Parser* parser);

Request* parser_parse(Parser* parser, Buffer* buf);

#endif