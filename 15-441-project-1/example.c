/* C declarations used in actions */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "parse.h"

int main(int argc, char **argv){
  //Read from the file the sample
  int fd_in = open(argv[1], O_RDONLY);
	if(fd_in < 0) {
		printf("Failed to open the file\n");
		return 0;
	}
  Buffer buf;
  buffer_init(&buf);
  int readRet = read(fd_in,buffer_input_ptr(&buf), buffer_input_size(&buf));
  if (readRet > 0) {
    buffer_input(&buf, readRet);
  }
  //Parse the buffer to the parse function. You will need to pass the socket fd and the buffer would need to
  //be read from that fd
  Parser parser; 
  parser_init(&parser);
  Request *request = parser_parse(&parser, &buf);
  if (request != NULL) {
    //Just printing everything
    printf("Http Method %s\n",request->http_method);
    printf("Http Version %s\n",request->http_version);
    printf("Http Uri %s\n",request->abs_path);
    RequestHeader* header = request->headers;
    for(; header != NULL; header = header->next){
      printf("Request Header\n");
      printf("Header name %s Header Value %s\n",header->header_name,header->header_value);
    }
    printf("%d\n", request->content_length);
    free(request->headers);
    free(request);
  }
  return 0;
}
