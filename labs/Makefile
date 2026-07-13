.PHONY: all test clean

all: test

test:
	$(MAKE) -C ring-buffer test
	$(MAKE) -C byte-stream-parser test
	$(MAKE) -C host-config-journal test

clean:
	$(MAKE) -C ring-buffer clean
	$(MAKE) -C byte-stream-parser clean
	$(MAKE) -C host-config-journal clean
