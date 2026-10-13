CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = bin/lsv1.0.0
SOURCE = src/ls-v1.0.0.c
OBJECT = obj/ls-v1.0.0.o

all: $(TARGET)

$(TARGET): $(OBJECT)
	$(CC) $(CFLAGS) $(OBJECT) -o $(TARGET)

$(OBJECT): $(SOURCE)
	$(CC) $(CFLAGS) -c $(SOURCE) -o $(OBJECT)

clean:
	rm -f $(OBJECT) $(TARGET)

.PHONY: all clean
