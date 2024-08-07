#include "./build/wlr-virtual-pointer.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/input.h>
#include <linux/prctl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

struct ClientState {
    struct wl_display*                      display;
    struct wl_registry*                     registry;
    struct zwlr_virtual_pointer_manager_v1* pointer_manager;
    struct zwlr_virtual_pointer_v1*         virtual_pointer;
    int                                     click_interval_ns;
    bool                                    key_pressed;
};

static int get_keyboard_input(int fd) {
    struct input_event ev;
    ssize_t            n = read(fd, &ev, sizeof(ev));

    if (n == -1 && errno != EAGAIN) {
        perror("read");
        return -1;
    }

    if (n == sizeof(ev) && ev.type == EV_KEY && ev.code == KEY_F8)
        return ev.value; // 1 for press, 0 for release

    return -1;
}

static const char* get_keyboard_device() {
    FILE*       fp = fopen("/proc/bus/input/devices", "r");
    static char device_file[20];
    char        line[256];

    if (!fp) {
        perror("Error opening /proc/bus/input/devices");
        return NULL;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "sysrq")) {
            snprintf(device_file, sizeof(device_file), "/dev/input/%s", strstr(line, "event"));
            device_file[strcspn(device_file, " ")] = '\0';
            fclose(fp);
            printf("Found keyboard device: %s\n", device_file);
            return device_file;
        }
    }

    fclose(fp);
    return NULL;
}

static void registry_global(void* data, struct wl_registry* registry, uint32_t name,
                            const char* interface, uint32_t version) {
    struct ClientState* state = data;

    if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        state->pointer_manager =
            wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
    }
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {}

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

static void send_click(struct ClientState* state, int button) {
    switch (button) {
    case 0:
        button = BTN_LEFT;
        break;
    case 1:
        button = BTN_RIGHT;
        break;
    case 2:
        button = BTN_MIDDLE;
        break;
    default:
        button = BTN_LEFT;
        break;
    }
    zwlr_virtual_pointer_v1_button(state->virtual_pointer, timestamp(), button,
                                   WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(state->virtual_pointer);
    wl_display_flush(state->display);

    zwlr_virtual_pointer_v1_button(state->virtual_pointer, timestamp(), button,
                                   WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(state->virtual_pointer);
    wl_display_flush(state->display);
}

static const struct option long_options[] = {{"toggle", no_argument, NULL, 't'},
                                             {"help", no_argument, NULL, 'h'},
                                             {"button", required_argument, NULL, 'b'},
                                             {"nosleep", no_argument, NULL, 'n'},
                                             {0, 0, 0, 0}};

static const char usage[] =
    "Usage: wl-clicker [clicks-per-second] [options]\n"
    "\n"
    "  -b  --button <0|1|2>    Specify which mouse button to click\n"
    "                          (0 for left, 1 for right, 2 for middle)\n"
    "  -t, --toggle            Toggle the autoclicker on keypress\n"
    "  -h, --help              Show this menu\n"
    "  -n, --nosleep           Disables sleeping in the main loop\n"
    "                          to click as fast as possible.\n"
    "                          Note this will increase CPU usage massively.\n"
    "\n";

int main(int argc, char* argv[]) {
    unsigned int clicks_per_second = 1;
    int          button_type = 0;
    bool         toggle_click = false;
    bool         no_sleep = false;

    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "thnb:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'h': // help
            printf("%s", usage);
            exit(EXIT_SUCCESS);
            break;
        case 't': // toggle
            toggle_click = true;
            break;
        case 'b': // button
            button_type = atoi(optarg);
            break;
        case 'n': // nosleep
            no_sleep = true;
            break;
        default:
            fprintf(stderr, "%s", usage);
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (optind < argc)
        clicks_per_second = abs(atoi(argv[optind]));

    if (prctl(PR_SET_TIMERSLACK, 1) == -1) {
        perror("prctl");
        exit(EXIT_FAILURE);
    }

    struct ClientState state = {0};

    state.click_interval_ns = (1e9 / clicks_per_second) - 10000 /* ??? */;

    const char* KBD_DEVICE = get_keyboard_device();
    if (!KBD_DEVICE) {
        fprintf(stderr, "Error: failed to find keyboard device.\n");
        return 1;
    }

    state.display = wl_display_connect(NULL);
    if (!state.display) {
        fprintf(stderr, "Error: failed to connect to Wayland display.\n");
        return 1;
    }

    state.registry = wl_display_get_registry(state.display);
    wl_registry_add_listener(state.registry, &registry_listener, &state);
    wl_display_roundtrip(state.display);

    if (!state.pointer_manager) {
        fprintf(stderr, "Error: your compositor does not support wlr-virtual-pointer.\n");
        return 1;
    }

    state.virtual_pointer =
        zwlr_virtual_pointer_manager_v1_create_virtual_pointer(state.pointer_manager, NULL);

    int kbd_fd = open(KBD_DEVICE, O_RDONLY);
    if (kbd_fd == -1) {
        perror("Error: failed to open keyboard device");
        return 1;
    }

    int flags = fcntl(kbd_fd, F_GETFL, 0);
    fcntl(kbd_fd, F_SETFL, flags | O_NONBLOCK);

    struct timespec sleep_time;
    struct timespec last_click_time = {0, 0};
    struct timespec current_time;

    if (state.click_interval_ns <= 1000000) {
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = state.click_interval_ns;
    } else {
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 1000000;
    }

    printf("Ready\n");

    while (1) {
        int key_state = get_keyboard_input(kbd_fd);

        if (key_state != -1) {
            if (toggle_click && key_state == 1)
                state.key_pressed = !state.key_pressed;
            else if (!toggle_click)
                state.key_pressed = key_state;
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        if (state.key_pressed) {
            long long elapsed_ns = (current_time.tv_sec - last_click_time.tv_sec) * 1e9 +
                                   (current_time.tv_nsec - last_click_time.tv_nsec);

            if (elapsed_ns >= state.click_interval_ns) {
                send_click(&state, button_type);
                last_click_time = current_time;
            }
        }

        if (!no_sleep)
            nanosleep(&sleep_time, NULL);
    }

    close(kbd_fd);
    zwlr_virtual_pointer_v1_destroy(state.virtual_pointer);
    zwlr_virtual_pointer_manager_v1_destroy(state.pointer_manager);
    wl_registry_destroy(state.registry);
    wl_display_disconnect(state.display);

    return 0;
}
