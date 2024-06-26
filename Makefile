CC = gcc
CFLAGS = -lwayland-client -linput -ludev -lrt
BINARY = ./build/wl-clicker
SOURCES = ./src/main.c ./src/protocols/wlr-virtual-pointer-unstable-v1-protocol.c

all: $(BINARY)

debug: CFLAGS += -g
debug: $(BINARY)

$(BINARY):
	mkdir -p ./build
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clear:
	rm -r ./build
