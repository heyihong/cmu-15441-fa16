#ifndef __HTTP_H__
#define __HTTP_H__

enum StatusCode {
	OK = 200,
	NOT_FOUND = 404,
	LENGTH_REQUIRED = 411,
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	SERVICE_UNAVAILABLE = 503,
	HTTP_VERSION_NOT_SUPPORTED = 505
};

// Request Header field
struct Request_header
{
	char header_name[4096];
	char header_value[4096];
	struct Request_header* next;
};

// Request
struct Request
{
	char http_version[50];
	char http_method[50];
	char http_uri[4096];
	struct Request_header *headers;	
	int content_length;
	char* message_body;
};

// Response Header field
struct Response_header
{
	char header_name[4096];
	char header_value[4096];
	struct Response_header* next;
};

// Response
struct Response
{
	char http_version[50];
	enum StatusCode status_code; 
	char reason_phrase[50];
	struct Response_header *headers;
};

typedef struct Request_header Request_header;

typedef struct Request Request;

typedef struct Response_header Response_header;

typedef struct Response Response;

typedef enum StatusCode StatusCode;

void request_init(Request* request);

void request_destroy(Request* request);

void request_add_header(Request* request, const char* name, const char* value);

Request_header* request_get_header(Request* request, const char* name);

void response_init(Response* response, StatusCode status_code);

void response_destroy(Response* response);

void response_add_header(Response* response, const char* name, const char* value);

#endif