all:
	mkdir -p ./build
	gcc -o ./build/wl-clicker \
		./src/main.c \
		./src/protocols/wlr-virtual-pointer-unstable-v1-protocol.c \
		-lwayland-client \
		-lrt

clear:
	rm -r ./build
