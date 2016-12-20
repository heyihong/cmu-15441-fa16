################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: lisod echo_client

CC = gcc
CFLAGS=-Wall -Werror

%.o : %.c
	$(CC) -c $(CCFLAGS) $<

lisod: pool.o lisod.o
	$(CC) -o lisod pool.o lisod.o ${CFLAGS}

echo_client: echo_client.o
	$(CC) -o echo_client echo_client.o ${CFLAGS}

clean:
	@rm *.o lisod echo_client
