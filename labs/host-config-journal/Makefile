CC ?= cc

CPPFLAGS ?=
CFLAGS ?=
LDFLAGS ?=
SANITIZERS ?= address,undefined

BASE_CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
SOURCES := config_journal.c test_config_journal.c
HEADERS := config_journal.h
TARGET := build/test_config_journal

.PHONY: all test sanitize clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(BASE_CFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

build:
	mkdir -p $@

test: $(TARGET)
	./$(TARGET)

sanitize: clean
	$(MAKE) test \
		CFLAGS="$(CFLAGS) -O1 -fno-omit-frame-pointer -fsanitize=$(SANITIZERS)" \
		LDFLAGS="$(LDFLAGS) -fsanitize=$(SANITIZERS)"

clean:
	rm -rf build
