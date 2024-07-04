#define _POSIX_C_SOURCE 200809L
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
#include <libinput.h>
#include "../build/wlr-virtual-pointer-unstable-v1-client-protocol.h"

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
    struct zwlr_virtual_pointer_v1 *virtual_pointer;
    int click_interval_us;
    bool key_pressed;
} client_state;

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    client_state *state = data;

    if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0)
        state->pointer_manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

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

static void sleep_us(long us) {
    struct timespec ts = {
        .tv_sec = us / 1000000,
        .tv_nsec = (us % 1000000) * 1000
    };
    nanosleep(&ts, NULL);
}

static void send_click(client_state *state) {
    zwlr_virtual_pointer_v1_button(state->virtual_pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(state->virtual_pointer);
    wl_display_flush(state->display);

    zwlr_virtual_pointer_v1_button(state->virtual_pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(state->virtual_pointer);
    wl_display_flush(state->display);
}

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);
    return fd < 0 ? -1 : fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

const static struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

const char* get_keyboard_device() {
    struct udev *udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Error: failed to create udev context\n");
        return NULL;
    }

    struct libinput *li = libinput_udev_create_context(&interface, NULL, udev);
    if (!li) {
        fprintf(stderr, "Error: failed to create libinput context\n");
        udev_unref(udev);
        return NULL;
    }

    if (libinput_udev_assign_seat(li, "seat0") != 0) {
        fprintf(stderr, "Error: failed to assign seat\n");
        libinput_unref(li);
        udev_unref(udev);
        return NULL;
    }

    struct pollfd fds = {
        .fd = libinput_get_fd(li),
        .events = POLLIN
    };

    int timeout_ms = 5000;
    bool device_found = false;
    static char keyboard_device[256];

    while (!device_found) {
        int ret = poll(&fds, 1, timeout_ms);
        if (ret == 0) {
            printf("Timeout reached, no keyboard device found.\n");
            break;
        }
        if (ret < 0) {
            perror("poll");
            break;
        }

        libinput_dispatch(li);
        struct libinput_event *event;

        while (!device_found && (event = libinput_get_event(li)) != NULL) {
            struct libinput_device *device = libinput_event_get_device(event);
            if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD) && strstr(libinput_device_get_name(device), "Keyboard")) {
                const char *devnode = libinput_device_get_sysname(device);
                printf("Found keyboard device: /dev/input/%s\n", devnode);
                snprintf(keyboard_device, sizeof(keyboard_device), "/dev/input/%s", devnode);
                device_found = true;
            }
            libinput_event_destroy(event);
        }
    }

    if (li) libinput_unref(li);
    if (udev) udev_unref(udev);
    
    return device_found ? keyboard_device : NULL;
}

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
        int poll_timeout = state.key_pressed ? state.click_interval_us / 1000 : -1;
        int ret = poll(fds, 2, poll_timeout);
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
            if (key_state != -1) {
                state.key_pressed = key_state;
            }
        }

        if (state.key_pressed) {
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            long time_diff_us = (current_time.tv_sec - last_click_time.tv_sec) * 1000000 + (current_time.tv_nsec - last_click_time.tv_nsec) / 1000;

            if (time_diff_us >= state.click_interval_us) {
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
