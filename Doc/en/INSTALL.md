# Installation and build

[English](INSTALL.md) | [Русский](../ru/INSTALL.md) | [CLI](CLI.md) | [Hardware](HARDWARE.md)

## Supported release and platform

The ready-made release is **WeAct35 1.0.0 for Debian ARM64**. Hardware and
system installation were confirmed on Orange Pi 3 LTS with Debian 13 ARM64.
Other CPU architectures may be able to build from source, but no ready-made
package or hardware qualification is claimed for them.

The packages are:

| Package | Purpose |
|---|---|
| `libweact1` | Runtime `libweact.so.1` |
| `libweact-dev` | Public headers, static library, `weact.pc`, C example |
| `weactterm` | `weact-cli`, `weact-list`, `weactterm`, FS35 feature tool |

The two `-dbgsym` packages are optional debug-symbol packages.

## Install the ARM64 packages

From the top-level source export:

```sh
cd Release/arm64
sha256sum -c SHA256SUMS
sudo apt install ./libweact1_1.0.0_arm64.deb ./libweact-dev_1.0.0_arm64.deb ./weactterm_1.0.0_arm64.deb
```

`apt` resolves declared runtime dependencies. The release uses FreeType,
GDK-Pixbuf/GLib, libvterm, `libqrencode4`, fontconfig, and DejaVu fonts.
Do not install a package when its checksum, version, or architecture differs
from the catalog.

Verify installation:

```sh
dpkg-query -W -f='${binary:Package} ${Version} ${Architecture} ${Status}\n' libweact1 libweact-dev weactterm
pkg-config --modversion weact
command -v weact-cli weact-list weactterm
weact-cli /?
weact-list --help
man weact-cli
man weact-list
man weactterm
man 3 libweact
```

Expected package version is `1.0.0`, architecture is `arm64`, and pkg-config
reports `1.0.0`.

`weactterm` installs the three section-1 program pages; `libweact-dev`
installs the section-3 library overview. Full bilingual guides remain in the
source/documentation tree.

Installed paths include `/usr/bin/weact-cli`, `/usr/bin/weact-list`,
`/usr/bin/weactterm`, `/usr/lib/aarch64-linux-gnu/libweact.so.1`,
`/usr/include/weact/`, and
`/usr/lib/aarch64-linux-gnu/pkgconfig/weact.pc`.

To remove the packages without changing unrelated packages:

```sh
sudo apt remove weactterm libweact-dev libweact1
```

Review the proposed package transaction before confirming it.

## USB discovery and access

List candidates before any hardware command:

```sh
weact-list --verbose
```

`weact-list` reads sysfs only. It recognizes VID:PID `1a86:fe0c` and selects a
model by the Product string. Use the reported `/dev/serial/by-id/...` symlink
with `weact-cli`; it remains stable when tty numbering changes.

The user running a hardware command needs read/write access to the selected
`/dev/ttyACM*` node. Check this rather than changing permissions blindly:

```sh
ls -l /dev/ttyACM0
id
```

If access or device-management policy must change, use the distribution's
normal administrator process. Do not bypass ownership checks or open an
unconfirmed port. See [Hardware](HARDWARE.md) and [CLI](CLI.md).

## Build from this source tree

Build dependencies are GNU make/GCC, pkg-config, `libfreetype-dev`,
`libvterm-dev` exactly 0.3.3, libqrencode development support, GDK-Pixbuf/GLib
development support, fontconfig, debhelper-compat 13, and Python 3 for Debian
packaging. The project Makefile checks FreeType and libvterm 0.3.3.

Keep generated files outside the export:

```sh
build=$(mktemp -d)
make BUILD_DIR="$build/obj" BIN_DIR="$build/bin" all
LD_LIBRARY_PATH="$build/obj" "$build/bin/weact-cli" /?
```

This produces `libweact.so.1.0.0`, `libweact.a`, `weact-cli`, `weact-list`,
`weactterm`, and `weact-fs35-features` below `$build`. The public source export
intentionally excludes its test harness; release regression is performed in
the project regression environment.

To build Debian binary packages from the export:

```sh
dpkg-buildpackage -b -us -uc
```

This command creates build outputs and packages; it is not an installation.
It is supported only after the required build dependencies are available.

## Common installation problems

| Symptom | Check |
|---|---|
| `pkg-config` cannot find `weact` | Install `libweact-dev`, then inspect `PKG_CONFIG_PATH` for conflicting overrides. |
| CLI cannot open the device | First use `weact-list --verbose`; verify the exact product, selected by-id path, permissions, and no visible owner. |
| FS35 starts with a changing demo | Run standalone `/init` once, then regional codes. |
| QR/Barcode region is rejected | Regions need FS35 and all `/x /y /w /h`; FS096 is fullscreen-only. |
| A source build fails at vterm check | Install the required libvterm 0.3.3 development package; do not silently substitute another ABI. |

Public distribution remains pending confirmation of GPL adaptation distribution
rights by the project owner.
