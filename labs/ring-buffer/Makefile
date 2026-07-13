CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
SANITIZERS ?= address,undefined

TARGET := test_ring_buffer
SOURCES := ring_buffer.c test_ring_buffer.c

.PHONY: all test sanitize clean

all: test

$(TARGET): $(SOURCES) ring_buffer.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@

test: $(TARGET)
	./$(TARGET)

sanitize: clean
	$(CC) $(CFLAGS) -O1 -fsanitize=$(SANITIZERS) -fno-omit-frame-pointer \
		$(SOURCES) -o $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
