# sys-ftpd-ovl

A [Tesla](https://github.com/WerWolv/Tesla-Menu) / [Ultrahand](https://github.com/ppkantorski/Ultrahand-Overlay) overlay for controlling [sys-ftpd](https://github.com/cathery/sys-ftpd) on Nintendo Switch.

## Features

- Displays your Switch's IP address and FTP port at a glance
- Live start and stop of sys-ftpd without rebooting
- Toggle sys-ftpd on or off at boot
- Configure username, password, port, and anonymous login directly from the overlay
- Password generator with configurable length (4–12 characters), uppercase, and special character options
- LED indicator toggle
- All settings are saved to `sdmc:/config/sys-ftpd/config.ini`

## Requirements

Atmosphere 1.10.0 or newer, [Ultrahand](https://github.com/ppkantorski/Ultrahand-Overlay) or [Tesla Menu](https://github.com/WerWolv/Tesla-Menu) with [nx-ovlloader](https://github.com/WerWolv/nx-ovlloader), and sys-ftpd installed.

## Installing sys-ftpd

Download the latest release of [sys-ftpd](https://github.com/cathery/sys-ftpd/releases) and copy the contents to the root of your SD card. The sysmodule files should end up at:

```
sd:/atmosphere/contents/420000000000000E/exefs.nsp
sd:/atmosphere/contents/420000000000000E/toolbox.json
```

Reboot into Atmosphere for sys-ftpd to start for the first time.

## Installing the overlay

Download `sys-ftpd-ovl.ovl` from the releases page and place it at:

```
sd:/switch/.overlays/sys-ftpd-ovl.ovl
```

Then open Ultrahand or Tesla Menu on your Switch.

## Configuration

Settings are read from and written to `sdmc:/config/sys-ftpd/config.ini`. You can add your own username presets by editing the `users` key under `[Presets]` as a comma-separated list:

```ini
[Presets]
users:=switch,ftpd,myname
ports:=2121,5000
```

Remember to turn the FTP server off and on after making changes for them to take effect.

## Building

### Prerequisites

[devkitPro](https://devkitpro.org/wiki/Getting_Started) with `switch-dev` and libnx 4.10.0 or newer.

### Setup

```bash
git clone https://github.com/WerWolv/libtesla libs/libtesla
wget https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h -O include/stb_truetype.h
cp libs/libtesla/include/tesla.hpp include/tesla.hpp
```

Then patch `tesla.hpp` for libnx 4.12 / gcc 15 compatibility:

```bash
awk 'NR==794 {print "                    u64 key = (static_cast<u64>(currCharacter) << 32) | static_cast<u64>(monospace) << 31 | static_cast<u64>(reinterpret_cast<u32&>(fontSize));"; next} {print}' include/tesla.hpp > include/tesla.hpp.tmp && mv include/tesla.hpp.tmp include/tesla.hpp

awk '{print; if (/struct ScissoringConfig \{/) {print "            ScissoringConfig() = default;"; print "            ScissoringConfig(s32 x, s32 y, s32 w, s32 h) : x(x), y(y), w(w), h(h) {}"}}' include/tesla.hpp > include/tesla.hpp.tmp && mv include/tesla.hpp.tmp include/tesla.hpp
```

Then build:

```bash
make
```

## Credits

[WerWolv](https://github.com/WerWolv) for libtesla and Tesla Menu, [ppkantorski](https://github.com/ppkantorski) for Ultrahand, and [cathery](https://github.com/cathery) for sys-ftpd.
