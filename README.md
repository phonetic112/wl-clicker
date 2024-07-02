# wl-clicker

An autoclicker for Wayland compositors that support the [wlr-virtual-pointer](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) protocol.

## Setup

Ensure the dependencies are installed:

```sh
sudo pacman -S wayland libinput linux-api-headers wlr-protocols
```

Compile with `make`.

Your user may need to be added to the `input` group. Check with:

```sh
groups [user]
```

If `input` isn't listed, add your user to it:

```sh
sudo usermod -aG input [user]
```

## Usage

```sh
wl-clicker [clicks-per-second]
```

Then, hold down F8 to start clicking.