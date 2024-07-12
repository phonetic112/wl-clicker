# wl-clicker

A simple and *insanely fast* autoclicker for Wayland. 

It works with compositors that support the [wlr-virtual-pointer](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) protocol.

## Setup

Ensure the dependencies are installed:

```sh
sudo pacman -S wayland linux-api-headers wlr-protocols
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
wl-clicker [clicks-per-second] [options]
```

Use `wl-clicker -h` to see options.

Then, hold down F8 to start clicking.

The fastest I could go in Firefox was around 1800 cps:

![image](https://github.com/user-attachments/assets/fa7fe032-e69d-406e-962e-f67af8f227cb)

Note that actual click speed may be slightly lower than requested due to compositor/app limitations.
