################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

CC = gcc
CFLAGS=-g -Wall
CPPFLAGS = -I. -I/usr/local/opt/openssl/include
LDFLAGS = -lssl -L/usr/local/opt/openssl/lib
DEPS = parse.h y.tab.h

default: all

all: example lisod

%.o : %.c $(DEPS)
	$(CC) -c $(CFLAGS) $< $(CPPFLAGS)

lex.yy.c: lexer.l
	flex $^

y.tab.c: parser.y
	yacc -d $^

example: y.tab.o lex.yy.o utils.o http.o buffer.o parse.o example.o
	$(CC) ${CFLAGS} -o $@ $^ ${LDFLAGS} 

lisod: log.o y.tab.o lex.yy.o utils.o http.o buffer.o parse.o handle.o cgi.o conn.o pool.o lisod.o
	$(CC) ${CFLAGS} -o $@ $^ ${LDFLAGS}

clean:
	@rm -f *.o lisod echo_client example lex.yy.c y.tab.c y.tab.h

handin: clean
	cd .. && tar cvf handin.tar 15-441-project-1 && cd -
