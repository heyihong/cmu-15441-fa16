#include "parse.h"
#include "utils.h"

void parser_init(Parser* parser) {
	parser->buf_len = 0;
	parser->state = STATE_START;
}

/**
* Given a char buffer returns the parsed request
*/
Request * parser_parse(Parser* parser, Buffer* buf) {
  	// Differant states in the state machine
	char ch;

	if (parser->buf_len == HTTP_HEADER_MAX_SIZE) {
		parser_init(parser);	
	}

	while (parser->state != STATE_CRLFCRLF && !buffer_is_empty(buf) && parser->buf_len < HTTP_HEADER_MAX_SIZE) {
		char expected = 0;

		ch = *buffer_output_ptr(buf);
		buffer_output(buf, 1);
		parser->buf[parser->buf_len++] = ch;

		switch (parser->state) {
			case STATE_START: case STATE_CRLF:
				expected = '\r';
				break;
			case STATE_CR: case STATE_CRLFCR:
				expected = '\n';
				break;
			default:
				parser->state = STATE_START;
				continue;
		}

		if (ch == expected) {
			parser->state++;
		} else {
			parser->state = STATE_START;
		}
	}
	if (parser->state != STATE_CRLFCRLF) {	
		if (parser->buf_len == HTTP_HEADER_MAX_SIZE) {
			parser->state = STATE_FAILED;
		}
		return NULL;
	}
	Request* request = (Request *) malloc(sizeof(Request));
    //TODO You will need to handle resizing this in parser.y
    request_init(request);
    parser->buf[parser->buf_len] = 0;
	set_parsing_options(parser->buf, parser->buf_len, request);
	int success = yyparse();
	if (success == SUCCESS) {
		RequestHeader* header = request_get_header(request, "Content-length");	
		if (header != NULL) {
			request->content_length = atoi(header->header_value);
		}
		// TODO check valid number
		if (request->content_length < 0) {
			success = !SUCCESS;
		}
	}
	if (success != SUCCESS) {
		request_destroy(request);
		free(request);
		parser->state = STATE_FAILED;
		return NULL;
	}
	return request;
}
