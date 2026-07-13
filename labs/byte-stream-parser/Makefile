CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
SANITIZERS ?= address,undefined

TARGET := test_stream_parser
SOURCES := stream_parser.c test_stream_parser.c

.PHONY: all test sanitize clean

all: test

$(TARGET): $(SOURCES) stream_parser.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@

test: $(TARGET)
	./$(TARGET)

sanitize: clean
	$(CC) $(CFLAGS) -O1 -fsanitize=$(SANITIZERS) -fno-omit-frame-pointer \
		$(SOURCES) -o $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
