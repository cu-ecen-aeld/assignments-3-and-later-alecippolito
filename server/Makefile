CC = gcc
CROSS_COMPILE ?= 
CFLAGS = -Wall -Wextra

all: aesdsocket

aesdsocket: aesdsocket.o
	$(CROSS_COMPILE)$(CC) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -g -c aesdsocket.c -o aesdsocket.o

clean:
	rm -f aesdsocket.o aesdsocket
	

