# Installation and build

[English](INSTALL.md) | [Русский](../ru/INSTALL.md) | [CLI](CLI.md) | [Hardware](HARDWARE.md)

## Supported release and platform

WeAct35 1.0.0-3 packages are available for Debian 13 `amd64`, Debian 13
`arm64`, and Ubuntu 24.04.5 `riscv64` in the [published GitHub Release
v1.0.0-3](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3). Each
architecture has five binary packages and a separate SHA-256 manifest. The
AMD64 packages were downloaded from the Release and passed checksum
verification; selected hardware scenarios were visually confirmed on Debian 13.

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
sha256sum -c SHA256SUMS-arm64.txt
sudo apt install ./libweact1_1.0.0-3_arm64.deb ./libweact-dev_1.0.0-3_arm64.deb ./weactterm_1.0.0-3_arm64.deb
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

Expected Debian package version is `1.0.0-3`, architecture is `arm64`, and
pkg-config reports upstream API version `1.0.0`.

## Install the AMD64 packages

Download the five AMD64 packages and `SHA256SUMS-amd64.txt` from [GitHub
Release v1.0.0-3](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3)
into one directory. Verify the complete set before installing:

```sh
sha256sum -c SHA256SUMS-amd64.txt
sudo apt install ./libweact1_1.0.0-3_amd64.deb ./libweact-dev_1.0.0-3_amd64.deb ./weactterm_1.0.0-3_amd64.deb
```

The two `-dbgsym` packages are optional. `apt` resolves runtime dependencies
from the configured Debian 13 repositories. Check the installed package
version and architecture with:

```sh
dpkg-query -W -f='${binary:Package} ${Version} ${Architecture} ${Status}\n' libweact1 libweact-dev weactterm
```

Expected values are Debian package version `1.0.0-3` and architecture
`amd64`. For the shared release catalog and package checksums, see the
[1.0.0-3 release notes](RELEASE-1.0.0-3.md).

`weactterm` installs the three section-1 program pages; `libweact-dev`
installs the section-3 library overview. Full bilingual guides remain in the
source/documentation tree.

On ARM64, installed paths include `/usr/bin/weact-cli`, `/usr/bin/weact-list`,
`/usr/bin/weactterm`, `/usr/lib/aarch64-linux-gnu/libweact.so.1`,
`/usr/include/weact/`, and
`/usr/lib/aarch64-linux-gnu/pkgconfig/weact.pc`.

To remove the packages without changing unrelated packages:

```sh
sudo apt remove weactterm libweact-dev libweact1
```

Review the proposed package transaction before confirming it.

## RISC-V RV2 package status

Orange Pi RV2 with Ubuntu 24.04.5 LTS has an installed, hardware-smoke-tested `riscv64` package revision `1.0.0-3` of upstream `1.0.0`. Obtain its five release packages and `SHA256SUMS-riscv64.txt` together, verify them, then install the three non-debug `.deb` files with `apt`. ARM64 Debian 13 packages are also revision `1.0.0-3`.

See [RISC-V release notes](RELEASE-1.0.0-3.md) and the [RV2 CDC ACM note](../../Misc/README.md).

## Debian 13 AMD64 source-build status

The same source tree builds natively on Debian 13 x86-64 (`amd64`), without
architecture-specific code changes. The shared/static library, `weact-cli`,
`weact-list`, `weact-fs35-features`, and `weactterm` were built with
`-Wall -Wextra -Werror -O2 -std=c11` and Debian hardening flags. Terminal/PTY
and related offline runtime regressions passed. Upstream API version
remains `1.0.0` and SONAME remains `libweact.so.1`.

The full build, including `weactterm`, requires `libvterm-dev` providing
libvterm exactly `0.3.3`; this dependency is now available and the full AMD64
source build has passed.
The existing Debian packaging uses `Architecture: any` and selects
`/usr/lib/x86_64-linux-gnu` on AMD64. The five 1.0.0-3 AMD64 binary packages passed checksum, lintian, ABI, and
dependency checks; `libweact1`, `libweact-dev`, and `weactterm` were then
installed and exercised by the user. FS35 clearing, text, QR, and QR region
output were visually confirmed. On FS096, true overlay preserved a displayed
QR while large text was drawn over it; fullscreen Code128 passed its first
cold-start after reconnect in three attempts with a new USB cable. One of two
extra attempts with an older cable showed image shift; this observation does
not establish a software defect. Other hardware scenarios remain unconfirmed.
The AMD64 packages are available in the published [GitHub Release
v1.0.0-3](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3); verify
them with `SHA256SUMS-amd64.txt` as described above. Use the existing
source-build commands below after satisfying all dependencies; ARM64/RISC-V
status is unchanged.

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
