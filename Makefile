CC = gcc
CFLAGS = -lwayland-client -D_POSIX_C_SOURCE=200809L -mshstk
LDFLAGS = -Wl,-z,relro,-z,now -Wl,-z,shstk
BINARY = ./build/wl-clicker
SOURCES = ./main.c ./build/wlr-virtual-pointer.c
PROTOCOL = /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml
PROTOCOL_C = ./build/wlr-virtual-pointer.c
PROTOCOL_H = ./build/wlr-virtual-pointer.h

all: $(BINARY)

debug: CFLAGS += -g
debug: $(BINARY)

$(BINARY): $(SOURCES) $(PROTOCOL_C) $(PROTOCOL_H)
	mkdir -p ./build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES)

$(PROTOCOL_C) $(PROTOCOL_H): $(PROTOCOL)
	mkdir -p ./build
	wayland-scanner client-header $(PROTOCOL) $(PROTOCOL_H)
	wayland-scanner private-code $(PROTOCOL) $(PROTOCOL_C)

clear:
	rm -rf ./build

.PHONY: all debug clear
