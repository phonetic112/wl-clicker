# wl-clicker

An autoclicker for Wayland compositors that support the `wlr-virtual-pointer` protocol.

## Usage

Compile with `make`.

```sh
wl-clicker [clicks-per-second] [keyboard-device]
```

Where `[keyboard-device]` is the keyboard device path from libinput. You can get the device path using the command `libinput list-devices | grep -A5 -E "Keyboard|keyboard"`.

Then, hold down F8 to start clicking.