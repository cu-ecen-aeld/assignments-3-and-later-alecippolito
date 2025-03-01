# Makefile
# Author: Alec Ippolito

# Compiler and flags
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -Werror

# Linker flags
LDFLAGS ?= -lpthread -lrt # Added -lrt here

# Target executable
TARGET ?= aesdsocket

# Source files
SRCS ?= aesdsocket.c

# Object files (can be automatically derived from SRCS)
OBJS = $(SRCS:.c=.o)

# Default rule
all: $(TARGET)

# Rule to build the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -g -o $(TARGET) $(OBJS) $(LDFLAGS)
        
# Rule to clean the build
clean:
	rm -f $(TARGET) $(OBJS)

# Rule to build object files from source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

