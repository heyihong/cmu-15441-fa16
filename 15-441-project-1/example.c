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
  char buf[8192];
	if(fd_in < 0) {
		printf("Failed to open the file\n");
		return 0;
	}
  int readRet = read(fd_in,buf,8192);
  //Parse the buffer to the parse function. You will need to pass the socket fd and the buffer would need to
  //be read from that fd
  Parser parser; 
  parser_init(&parser);
  int offset = 0;
  while (offset != readRet) {
    Request *request = parse(&parser, buf, &offset, readRet);
    if (request != NULL) {
      //Just printing everything
      printf("Http Method %s\n",request->http_method);
      printf("Http Version %s\n",request->http_version);
      printf("Http Uri %s\n",request->http_uri);
      Request_header* header = request->headers;
      for(; header != NULL; header = header->next){
        printf("Request Header\n");
        printf("Header name %s Header Value %s\n",header->header_name,header->header_value);
      }
      printf("%d\n", request->content_length);
      free(request->headers);
      free(request);
    }
  }
  parser_destroy(&parser);
  return 0;
}
