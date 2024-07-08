#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>
#include <ctype.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

int handle_keyboard_input(int fd);
const char* get_keyboard_device();

#endif /* INPUT_H */
