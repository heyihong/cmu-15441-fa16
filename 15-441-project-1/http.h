#ifndef __HTTP_H__
#define __HTTP_H__

#define HTTP_HEADER_MAX_SIZE (1 << 14)

enum StatusCode {
	OK = 200,
	BAD_REQUEST = 400,
	NOT_FOUND = 404,
	LENGTH_REQUIRED = 411,
	REQUEST_ENTITY_TOO_LARGE = 413,
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	SERVICE_UNAVAILABLE = 503,
	HTTP_VERSION_NOT_SUPPORTED = 505
};

// Request Header field
struct RequestHeader
{
	char* header_name;
	char* header_value;
	struct RequestHeader* next;
};

// Request
struct Request
{
	char* http_version;
	char* http_method;
	char* abs_path;
	char* query;
	struct RequestHeader *headers;	
	int content_length;
	char* message_body;
};

// Response Header field
struct ResponseHeader
{
	char* header_name;
	char* header_value;
	struct ResponseHeader* next;
};

// Response
struct Response
{
	char* http_version;
	enum StatusCode status_code; 
	char* reason_phrase;
	struct ResponseHeader *headers;
};

typedef struct RequestHeader RequestHeader;

typedef struct Request Request;

typedef struct ResponseHeader ResponseHeader;

typedef struct Response Response;

typedef enum StatusCode StatusCode;

void request_init(Request* request);

void request_destroy(Request* request);

void request_add_header(Request* request, const char* name, const char* value);

char* request_get_header(Request* request, const char* name);

int request_connection_close(Request* request);

void response_init(Response* response, StatusCode status_code);

Response* response_error(StatusCode code);

void response_destroy(Response* response);

void response_add_header(Response* response, const char* name, const char* value);

#endif