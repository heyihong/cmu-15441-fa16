#ifndef __PARSE_H__
#define __PARSE_H__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"

#define SUCCESS 0
#define PARSER_BUF_SIZE 8192

enum ParserState {
	STATE_START = 0, STATE_CR, STATE_CRLF, STATE_CRLFCR, STATE_CRLFCRLF
};

struct Parser {
	char buf[PARSER_BUF_SIZE];
	int buf_len;
	enum ParserState state;
	int body_offset;
	struct Request* request;
};

typedef struct Parser Parser;

void parser_init(Parser* parser);

void parser_destroy(Parser* parser);

Request* parse(Parser* parser, char *buffer, int* offset, int size);

#endif