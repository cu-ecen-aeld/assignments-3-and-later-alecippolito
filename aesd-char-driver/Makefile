CC = gcc
CFLAGS = -Wall -g
OBJ = aesd-circular-buffer.o
TARGET = aesd-circular-buffer

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ)

aesd-circular-buffer.o: aesd-circular-buffer.c
	$(CC) $(CFLAGS) -c aesd-circular-buffer.c

clean:
	rm -f $(OBJ) $(TARGET)

