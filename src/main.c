#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

struct state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
    struct zwlr_virtual_pointer_v1 *virtual_pointer;
    int click_interval_us;
    bool f8_pressed;
};

static void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    struct state *state = data;

    if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        state->pointer_manager = wl_registry_bind(registry, name,
            &zwlr_virtual_pointer_manager_v1_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
        uint32_t name) {
    ;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static int timestamp() {
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	int ms = 1000 * tp.tv_sec + tp.tv_nsec / 1000000;
	return ms;
}

static int handle_keyboard_input(int fd) {
    struct input_event ev;
    ssize_t n = read(fd, &ev, sizeof(ev));

    if (n == sizeof(ev)) {
        if (ev.type == EV_KEY && ev.code == KEY_F8) {
            return ev.value;  // 1 for press, 0 for release
        }
    } else if (n == -1 && errno != EAGAIN) {
        perror("read");
    }

    return -1;
}

static void send_click(struct zwlr_virtual_pointer_v1 *virtual_pointer) {
    zwlr_virtual_pointer_v1_button(virtual_pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(virtual_pointer);
    
    zwlr_virtual_pointer_v1_button(virtual_pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(virtual_pointer);
}

static void sleep_us(long us) {
    struct timespec ts = {
        .tv_sec = us / 1000000,
        .tv_nsec = (us % 1000000) * 1000
    };
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <clicks_per_second> <keyboard_device>\n", argv[0]);
        return 1;
    }

    struct state state = {0};
    int clicks_per_second = atoi(argv[1]);
    state.click_interval_us = 1000000 / clicks_per_second;
    const char *kbd_device = argv[2];

    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);

    if (!state.pointer_manager) {
        fprintf(stderr, "Compositor doesn't support wlr-virtual-pointer-unstable-v1\n");
        return 1;
    }

    state.virtual_pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
        state.pointer_manager, NULL);

    int kbd_fd = open(kbd_device, O_RDONLY);
    if (kbd_fd == -1) {
        perror("Failed to open keyboard device");
        return 1;
    }

    int flags = fcntl(kbd_fd, F_GETFL, 0);
    fcntl(kbd_fd, F_SETFL, flags | O_NONBLOCK);

    struct pollfd fds[2] = {
        {wl_display_get_fd(state.display), POLLIN, 0},
        {kbd_fd, POLLIN, 0}
    };

    struct timespec last_click_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_click_time);

    while (1) {
        if (poll(fds, 2, 0) > 0) {
            if (fds[0].revents & POLLIN) {
                if (wl_display_dispatch(state.display) == -1) {
                    break;
                }
            }
            if (fds[1].revents & POLLIN) {
                int key_state = handle_keyboard_input(kbd_fd);
                if (key_state != -1) {
                    state.f8_pressed = key_state;
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long time_diff_us = (current_time.tv_sec - last_click_time.tv_sec) * 1000000 +
                            (current_time.tv_nsec - last_click_time.tv_nsec) / 1000;

        if (state.f8_pressed && time_diff_us >= state.click_interval_us) {
            send_click(state.virtual_pointer);
            last_click_time = current_time;

            long sleep_time = state.click_interval_us - 1000;
            if (sleep_time > 0) {
                sleep_us(sleep_time);
            }
        }
    }

    close(kbd_fd);
    zwlr_virtual_pointer_v1_destroy(state.virtual_pointer);
    zwlr_virtual_pointer_manager_v1_destroy(state.pointer_manager);
    wl_registry_destroy(state.registry);
    wl_display_disconnect(state.display);

    return 0;
}