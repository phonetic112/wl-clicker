#include <src/wayland.h>
#include <src/input.h>
#include <getopt.h>

static const struct option long_options[] = {
    {"toggle", no_argument, NULL, 't'},
    {"help", no_argument, NULL, 'h'},
    {"button", required_argument, NULL, 'b'},
    {0, 0, 0, 0}
};

static const char usage[] =
    "Usage: wl-clicker [clicks-per-second] [options]\n"
    "\n"
    "  -b  --button <0|1|2>    Specify which mouse button to click (0 for left, 1 for right, 2 for middle)\n"
    "  -t, --toggle            Toggle the autoclicker on keypress\n"
    "  -h, --help              Show this menu\n"
    "\n";

int main(int argc, char *argv[]) {
    unsigned int clicks_per_second = 1;
    bool toggle_key = false;
    int button_to_press = 0;

    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "th:b:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 'h': // help
                printf("%s", usage);
                exit(EXIT_SUCCESS);
                break;
            case 't': // toggle
                toggle_key = true;
                break;
            case 'b':
                button_to_press = atoi(optarg);
                break;
            default:
                fprintf(stderr, "%s", usage);
                exit(EXIT_FAILURE);
                break;
        }
    }

    if (optind < argc)
        clicks_per_second = abs(atoi(argv[optind]));

    client_state state = {0};

    state.click_interval_us = 1000000 / clicks_per_second;

    const char *kbd_device = get_keyboard_device();
    if (!kbd_device) {
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
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double time_since_last_click =
            (current_time.tv_sec - last_click_time.tv_sec) * 1000000.0 +
            (current_time.tv_nsec - last_click_time.tv_nsec) / 1000.0;

        struct timespec timeout = {0, 0};
        if (state.key_pressed) {
            double wait_time = state.click_interval_us - time_since_last_click;
            if (wait_time > 0) {
                timeout.tv_sec = (time_t)(wait_time / 1000000.0);
                timeout.tv_nsec = (long)((wait_time - (timeout.tv_sec * 1000000.0)) * 1000.0);
            }
        } else {
            timeout.tv_sec = 1;
        }

        int ret = ppoll(fds, 2, state.key_pressed ? &timeout : NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("ppoll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(state.display) == -1)
                break;
        }
        if (fds[1].revents & POLLIN) {
            int key_state = handle_keyboard_input(kbd_fd);
            if (key_state != -1) {
                if (toggle_key) {
                    if (key_state == 1)
                        state.key_pressed = !state.key_pressed;
                } else
                    state.key_pressed = key_state;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        time_since_last_click =
            (current_time.tv_sec - last_click_time.tv_sec) * 1000000.0 +
            (current_time.tv_nsec - last_click_time.tv_nsec) / 1000.0;

        if (state.key_pressed && time_since_last_click >= state.click_interval_us) {
            send_click(&state, button_to_press);
            last_click_time = current_time;
        }
    }

    close(kbd_fd);
    zwlr_virtual_pointer_v1_destroy(state.virtual_pointer);
    zwlr_virtual_pointer_manager_v1_destroy(state.pointer_manager);
    wl_registry_destroy(state.registry);
    wl_display_disconnect(state.display);

    return 0;
}
