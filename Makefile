CC = gcc
CFLAGS = -lwayland-client -linput -ludev -lrt -I./include -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
BINARY = ./build/wl-clicker
SOURCES = ./src/main.c \
	./src/wayland.c \
	./src/input.c \
	./build/wlr-virtual-pointer-unstable-v1-protocol.c
PROTOCOL = /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml

all: $(BINARY)

debug: CFLAGS += -g
debug: $(BINARY)

$(BINARY):
	mkdir -p ./build
	wayland-scanner client-header \
		$(PROTOCOL) \
		./build/wlr-virtual-pointer-unstable-v1-client-protocol.h
	wayland-scanner private-code \
		$(PROTOCOL) \
		./build/wlr-virtual-pointer-unstable-v1-protocol.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clear:
	rm -r ./build

.PHONY: all debug clear
