#include <stdbool.h>
#include <string.h>
#include <linux/input-event-codes.h>
#include <time.h>
#include <wayland-client.h>
#include "../build/wlr-virtual-pointer-unstable-v1-client-protocol.h"

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
    struct zwlr_virtual_pointer_v1 *virtual_pointer;
    int click_interval_us;
    bool key_pressed;
} client_state;

void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name);
void send_click(client_state *state);
extern const struct wl_registry_listener registry_listener;