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
DEPS = parse.h y.tab.h

default: all

all: example lisod echo_client

%.o : %.c $(DEPS)
	$(CC) -c $(CFLAGS) $<

lex.yy.c: lexer.l
	flex $^

y.tab.c: parser.y
	yacc -d $^

example: y.tab.o lex.yy.o utils.o http.o parse.o example.o
	$(CC) ${CFLAGS} -o $@ $^ -I.

lisod: log.o y.tab.o lex.yy.o utils.o http.o parse.o pool.o lisod.o
	$(CC) ${CFLAGS} -o $@ $^ -I.

echo_client: echo_client.o
	$(CC) ${CFLAGS} -o $@ $^ -I.

clean:
	@rm -f *.o lisod echo_client example lex.yy.c y.tab.c y.tab.h

handin: clean
	cd .. && tar cvf handin.tar 15-441-project-1 && cd -
