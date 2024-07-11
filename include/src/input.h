#ifndef INPUT_H
#define INPUT_H

#include <poll.h>
#include <stdbool.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <fcntl.h>

int handle_keyboard_input(int fd);
const char* get_keyboard_device();

#endif /* INPUT_H */
