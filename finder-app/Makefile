CC = gcc
CROSS_COMPILE ?= 

writer: writer.c
	$(CROSS_COMPILE)$(CC) -o writer writer.c
clean:
	rm -f writer *.o
all: writer

