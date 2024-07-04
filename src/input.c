#include "input.h"

int handle_keyboard_input(int fd) {
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
