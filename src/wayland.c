#include <src/wayland.h>

void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    client_state *state = data;

    if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        state->pointer_manager =
            wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
    }
}

void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static int timestamp() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    int ms = 1000 * tp.tv_sec + tp.tv_nsec / 1000000;
    return ms;
}

void send_click(client_state *state) {
    zwlr_virtual_pointer_v1_button(
        state->virtual_pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(state->virtual_pointer);
    wl_display_flush(state->display);

    zwlr_virtual_pointer_v1_button(
        state->virtual_pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(state->virtual_pointer);
    wl_display_flush(state->display);
}
