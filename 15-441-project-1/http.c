#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "utils.h"


Request_header* request_get_header(Request* request, const char* name) {
	Request_header* p = request->headers;
	while (p != NULL) {
		if (!strcmp(p->header_name, name)) {
			break;
		}
		p = p->next;
	}
	return p;
}

void request_init(Request* request) {
	memset(request->http_version, 0, sizeof(request->http_version));
	memset(request->http_method, 0, sizeof(request->http_method));
	memset(request->http_uri, 0, sizeof(request->http_uri));
	request->headers = NULL;	
	request->content_length = 0;
	request->message_body = NULL;
}

void request_destroy(Request* request) {
	if (request->message_body != NULL) {
		free(request->message_body);
	}	
	Request_header* p = NULL;
	while (request->headers != NULL) {
		p = request->headers;
		request->headers = p->next;
		free(p);
	}
}

void request_add_header(Request* request, const char* name, const char* value) {
	Request_header* header = (Request_header*)malloc(sizeof(Request_header));
	strcpy(header->header_name, name);
	strcpy(header->header_value, value);	
	header->next = request->headers;
	request->headers = header;
}

void response_init(Response* response, StatusCode status_code) {
	response->headers = NULL	;
	strcpy(response->http_version, "HTTP/1.1");	
	response->status_code = status_code;
	switch (status_code) {
		case OK:
			strcpy(response->reason_phrase, "OK");
			break;
		case NOT_FOUND:
			strcpy(response->reason_phrase, "Not Found");
			break;
		case LENGTH_REQUIRED:
			strcpy(response->reason_phrase, "Length Required");
			break;
		case INTERNAL_SERVER_ERROR:
			strcpy(response->reason_phrase, "Internal Server Error");
			break;
		case NOT_IMPLEMENTED:
			strcpy(response->reason_phrase, "Not Implemented");
			break;
		case SERVICE_UNAVAILABLE:
			strcpy(response->reason_phrase, "Service Unavailable");
			break;
		case HTTP_VERSION_NOT_SUPPORTED:
			strcpy(response->reason_phrase, "HTTP Version not supported");
			break;
	}
	char str[4096];
	time_t now = time(0);
  	get_http_format_date(&now, str, 4096);
	// auto generate server and date header
	response_add_header(response, "Server", "Liso/1.0");
	response_add_header(response, "Date", str);
}

void response_destroy(Response* response) {
	Response_header* p = NULL;
	while (response->headers != NULL) {
		p = response->headers;
		response->headers = p->next;
		free(p);
	}
}

void response_add_header(Response* response, const char* name, const char* value) {
	Response_header* header = (Response_header*)malloc(sizeof(Response_header));
	strcpy(header->header_name, name);
	strcpy(header->header_value, value);
	header->next = response->headers;
	response->headers = header;
}
