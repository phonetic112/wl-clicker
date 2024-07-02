CC = gcc
CFLAGS = -lwayland-client -linput -ludev -lrt
BINARY = ./build/wl-clicker
SOURCES = ./src/main.c ./build/wlr-virtual-pointer-unstable-v1-protocol.c

all: $(BINARY)

debug: CFLAGS += -g
debug: $(BINARY)

$(BINARY):
	mkdir -p ./build
	wayland-scanner client-header /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml ./build/wlr-virtual-pointer-unstable-v1-client-protocol.h
	wayland-scanner private-code /usr/share/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml ./build/wlr-virtual-pointer-unstable-v1-protocol.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clear:
	rm -r ./build
