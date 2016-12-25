#include "parse.h"
#include "utils.h"

void parser_init(Parser* parser) {
	parser->buf_len = 0;
	parser->state = STATE_START;
	parser->body_offset = 0;
	parser->request = NULL;
}

void parser_destroy(Parser* parser) {
	if (parser->request != NULL) {
		request_destroy(parser->request);
		free(parser->request);
	}
}

/**
* Given a char buffer returns the parsed request
*/
Request * parse(Parser* parser, char *buffer, int* offset, int size) {
	Request *request = parser->request;
	if (request == NULL) {
	  	//Differant states in the state machine
		int i = *offset;
		char ch;

		if (parser->buf_len == PARSER_BUF_SIZE) {
			parser_init(parser);	
		}

		while (parser->state != STATE_CRLFCRLF &&  i < size && parser->buf_len < PARSER_BUF_SIZE) {
			char expected = 0;

			ch = buffer[i++];
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

			if (ch == expected)
				parser->state++;
			else {
				parser->state = STATE_START;
			}

		}

		*offset = i;

		if (parser->state != STATE_CRLFCRLF) {	
			return NULL;
		}
		request = (Request *) malloc(sizeof(Request));
	    //TODO You will need to handle resizing this in parser.y
	    request_init(request);
	    parser->buf[parser->buf_len] = 0;
		set_parsing_options(parser->buf, parser->buf_len, request);
		int success = yyparse();
		if (success == SUCCESS) {
			Request_header* header = request_get_header(request, "Content-length");	
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
			parser_init(parser);
			return NULL;
		}
		parser->request = request;
	}
	// parser->request is not NULL
	int copy_length = min(request->content_length - parser->body_offset, size - *offset);
	memcpy(request->message_body, buffer, copy_length);
	parser->body_offset += copy_length;
	*offset += copy_length;
	if (request->content_length == parser->body_offset) {
		parser->request = NULL;
		parser_init(parser);
		return request;
	}
	return NULL;
}
