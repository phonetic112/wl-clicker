CC = gcc
CFLAGS = -lwayland-client -I./include -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
BINARY = ./build/wl-clicker
SOURCES = ./src/main.c \
	./src/wayland.c \
	./src/input.c \
	./build/wlr-virtual-pointer-unstable-v1-protocol.c
PROTOCOL = /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml
PROTOCOL_C = ./build/wlr-virtual-pointer-unstable-v1-protocol.c
PROTOCOL_H = ./build/wlr-virtual-pointer-unstable-v1-client-protocol.h

all: $(BINARY)

debug: CFLAGS += -g
debug: $(BINARY)

$(BINARY): $(SOURCES) $(PROTOCOL_C) $(PROTOCOL_H)
	mkdir -p ./build
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

$(PROTOCOL_C) $(PROTOCOL_H): $(PROTOCOL)
	mkdir -p ./build
	wayland-scanner client-header $(PROTOCOL) $(PROTOCOL_H)
	wayland-scanner private-code $(PROTOCOL) $(PROTOCOL_C)

clear:
	rm -r ./build

.PHONY: all debug clear
