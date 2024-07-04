#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include "wayland.h"
#include "input.h"
#include "../build/wlr-virtual-pointer-unstable-v1-client-protocol.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <clicks_per_second>\n", argv[0]);
        return 1;
    }

    client_state state = {0};
    int clicks_per_second = atoi(argv[1]);
    state.click_interval_us = 1000000 / clicks_per_second;

    const char *kbd_device = get_keyboard_device();
    if (!kbd_device) {
        fprintf(stderr, "Error: failed to find keyboard device\n");
        return 1;
    }

    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Error: failed to connect to Wayland display\n");
        return 1;
    }

    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);

    if (!state.pointer_manager) {
        fprintf(stderr, "Error: compositor doesn't support wlr-virtual-pointer-unstable-v1\n");
        return 1;
    }

    state.virtual_pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(state.pointer_manager, NULL);

    int kbd_fd = open(kbd_device, O_RDONLY);
    if (kbd_fd == -1) {
        perror("Error: failed to open keyboard device");
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

    printf("Ready\n");

    while (1) {
        int ret = poll(fds, 2, state.key_pressed ? 0: -1);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(state.display) == -1) {
                break;
            }
        }
        if (fds[1].revents & POLLIN) {
            int key_state = handle_keyboard_input(kbd_fd);
            if (key_state != -1)
                state.key_pressed = key_state;
        }

        if (state.key_pressed) {
            clock_gettime(CLOCK_MONOTONIC, &current_time);

            if (((current_time.tv_sec - last_click_time.tv_sec) * 1000000 + (current_time.tv_nsec - last_click_time.tv_nsec) / 1000) >= state.click_interval_us) {
                send_click(&state);
                last_click_time = current_time;
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
