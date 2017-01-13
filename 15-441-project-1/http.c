#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "utils.h"

static char tmpbuf[4096];

RequestHeader* request_get_header(Request* request, const char* name) {
	RequestHeader* p = request->headers;
	while (p != NULL) {
		if (!strcmp(p->header_name, name)) {
			break;
		}
		p = p->next;
	}
	return p;
}

void request_init(Request* request) {
	request->http_version = NULL;
	request->http_method = NULL;
	request->abs_path = NULL;
	request->query = NULL;
	request->headers = NULL;	
	request->content_length = 0;
	request->message_body = NULL;
}

void request_destroy(Request* request) {
	free(request->http_version);
	free(request->http_method);
	free(request->abs_path);
	free(request->query);
	if (request->message_body != NULL) {
		free(request->message_body);
	}	
	RequestHeader* p = NULL;
	while (request->headers != NULL) {
		p = request->headers;
		free(p->header_name);
		free(p->header_value);
		request->headers = p->next;
		free(p);
	}
}

void request_add_header(Request* request, const char* name, const char* value) {
	RequestHeader* header = (RequestHeader*)malloc(sizeof(RequestHeader));
	header->header_name = new_str(name);
	header->header_value = new_str(value);
	header->next = request->headers;
	request->headers = header;
}

void response_init(Response* response, StatusCode status_code) {
	response->headers = NULL;
	response->http_version = new_str("HTTP/1.1");	
	response->status_code = status_code;
	switch (status_code) {
		case OK:
			response->reason_phrase = new_str("OK");
			break;
		case NOT_FOUND:
			response->reason_phrase = new_str("Not Found");
			break;
		case LENGTH_REQUIRED:
			response->reason_phrase = new_str("Length Required");
			break;
		case INTERNAL_SERVER_ERROR:
			response->reason_phrase = new_str("Internal Server Error");
			break;
		case NOT_IMPLEMENTED:
			response->reason_phrase = new_str("Not Implemented");
			break;
		case SERVICE_UNAVAILABLE:
			response->reason_phrase = new_str("Service Unavailable");
			break;
		case HTTP_VERSION_NOT_SUPPORTED:
			response->reason_phrase = new_str("HTTP Version not supported");
			break;
	}
	char* str = tmpbuf;
	time_t now = time(0);
  	get_http_format_date(&now, str, sizeof(tmpbuf));
	// auto generate server and date header
	response_add_header(response, "Server", "Liso/1.0");
	response_add_header(response, "Date", str);
	response_add_header(response, "Connection", "close");
}

void response_destroy(Response* response) {
	free(response->http_version);
	free(response->reason_phrase);
	ResponseHeader* p = NULL;
	while (response->headers != NULL) {
		p = response->headers;
		free(p->header_name);
		free(p->header_value);
		response->headers = p->next;
		free(p);
	}
}

void response_add_header(Response* response, const char* name, const char* value) {
	ResponseHeader* header = (ResponseHeader*)malloc(sizeof(ResponseHeader));
	header->header_name = new_str(name);
	header->header_value = new_str(value);
	header->next = response->headers;
	response->headers = header;
}
