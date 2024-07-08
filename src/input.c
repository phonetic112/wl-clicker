#include <src/input.h>

int handle_keyboard_input(int fd) {
    struct input_event ev;
    ssize_t n = read(fd, &ev, sizeof(ev));

    if (n == sizeof(ev)) {
        if (ev.type == EV_KEY && ev.code == KEY_F8)
            return ev.value;  // 1 for press, 0 for release
    } else if (n == -1 && errno != EAGAIN)
        perror("read");

    return -1;
}

const char* get_keyboard_device() {
    FILE* fp;
    char line[256];
    static char device_file[20];

    fp = fopen("/proc/bus/input/devices", "r");
    if (fp == NULL) {
        perror("Error opening /proc/bus/input/devices");
        return NULL;
    }

    while (fgets(line, sizeof(line), fp)) {
        char* event = strstr(line, "sysrq") ? strstr(line, "event") : NULL;
        if (event) {
            snprintf(device_file, sizeof(device_file), "/dev/input/%s", event);
            device_file[strcspn(device_file, " ")] = '\0';
            fclose(fp);
            printf("Found keyboard device: %s\n", device_file);
            return device_file;
        }
    }

    fclose(fp);
    return NULL;
}
