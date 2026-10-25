# WeActTerm guide

[English](TERMINAL.md) | [Русский](../ru/TERMINAL.md) | [Install](INSTALL.md) | [CLI](CLI.md) | [Hardware](HARDWARE.md)

`weactterm` is a foreground FS35 terminal presenter. It runs a shell or one
shell command in a POSIX PTY, lets libvterm maintain the terminal screen, and
rasterises that screen with FreeType into the 480x320 FS35 canvas. It is useful
for a small status console, an interactive maintenance shell, or a controlled
command transcript. It is not a replacement for a full desktop terminal: it
has no scrollback UI, no runtime font selection, no mouse protocol, and no
claim of complete terminal-emulator compatibility beyond libvterm's screen
model and the input handling documented below.

## Scope, display, and dependencies

Only FS35 (`Display FS V1`, 480x320 RGB565 little-endian) is supported. FS096
is rejected: its 160x80 legacy pixel format and geometry are not a terminal
target. The package uses libvterm 0.3.3, FreeType, DejaVu Sans Mono, a PTY,
and the guarded FS35 connection from `libweact`. In hardware mode it performs
the normal managed FS35 initialisation: orientation 02 is selected, confirmed,
and the full display is cleared black before terminal frames are sent.

## Syntax and modes

```text
weactterm [--dry-run | --run] [--allow-unknown-occupancy]
          [--command 'shell command'] [--ctrl-c-to-pty] [--perf-stats]
          [--device /dev/...] [--serial SERIAL]
          [--font-size tiny|small|normal|large|xlarge]
          [--compression off|fastlz]
          [--brightness 0..50] [--brightness-fade-ms 0..65535]
```

No argument starts a finite memory-only PTY demonstration; it does not open a
USB device. `--dry-run` explicitly selects the same mode. `--run` is the only
mode that opens FS35. The two mode options are mutually exclusive. `--help`
prints usage and exits 0; malformed/duplicate options and a fade without a
brightness value exit 2.

| Option | Meaning and limits |
|---|---|
| `--run` | Guarded FS35 hardware mode. One matching device must be selected. |
| `--dry-run` | Memory-only mode; useful for checking PTY, fonts and terminal rendering output. |
| `--command CMD` | Child executes `/bin/sh -c CMD`. Without it, dry-run uses its finite built-in demo; `--run` starts interactive `$SHELL`, falling back to `/bin/sh`. |
| `--device PATH` | Absolute candidate CDC ACM/by-id path, less than 4096 bytes. Prefer a path reported by `weact-list --verbose`. |
| `--serial SERIAL` | Require a matching USB serial, 1–63 bytes. Useful to disambiguate devices. |
| `--allow-unknown-occupancy` | Narrow override only for an otherwise verified FS35 when ownership scan is inconclusive from foreign-process permissions. It never accepts a known busy port, wrong Product, namespace/scanner failure, or failed lock. |
| `--font-size NAME` | `tiny`, `small`, `normal` (default), `large`, `xlarge`; see grid table below. |
| `--compression off|fastlz` | Default `off`. FastLZ is an explicit FS35 presenter mode; it is not a generic compression or recovery mechanism. |
| `--brightness N` | Explicit 0–50 percent FS35 backlight request after successful init. Omission leaves brightness unchanged. |
| `--brightness-fade-ms N` | Requires brightness; accepts 0–65535, while the protocol clamps effective fade to 5000 ms. |
| `--ctrl-c-to-pty` | Forward Ctrl+C to PTY instead of making the presenter exit. |
| `--perf-stats` | Print host-side timing/counter diagnostics to stderr on exit; LCD commit time is not measured. |

## Selecting hardware safely

First list devices without opening a tty:

```sh
weact-list --verbose
```

Confirm Product `Display FS V1`, your permissions, the stable path under
`/dev/serial/by-id/`, and that no other application owns it. Both WeAct models
share VID:PID `1a86:fe0c`, so VID:PID alone is not sufficient. A guarded run
may auto-select exactly one matching FS35; give `--device` or `--serial` when
there are several. Do not add `--allow-unknown-occupancy` merely to silence an
error: use it only after those checks show the intended device and no visible
owner.

```sh
weactterm --run --device /dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00
```

The placeholder is intentionally not a real serial number. Copy your own path
from `weact-list --verbose`.

## Text grid, colours, and rendering

The fixed font file is DejaVu Sans Mono. libvterm receives UTF-8, including
Cyrillic supported by the checked font, and `TERM=xterm` plus `LC_ALL=C.UTF-8`
are supplied to the child. The PTY winsize follows the selected grid.

| Size | Cell (pixels) | Grid (columns × rows) |
|---|---:|---:|
| `tiny` | 5×10 | 96×32 |
| `small` | 6×12 | 80×26 |
| `normal` | 8×16 | 60×20 |
| `large` | 12×24 | 40×13 |
| `xlarge` | 16×32 | 30×10 |

libvterm owns cursor, current screen, colours and supported control sequences;
FreeType blends glyphs onto the RGB565 FS35 canvas. The defaults are light
foreground (`224,224,224`) on black. The project does not promise colour
calibration, complex Unicode shaping, arbitrary fonts, resize-to-a-new-grid,
or a visual LCD result merely because host transmission succeeded.

## Input, PTY, signals, and lifecycle

When stdin is a terminal, `weactterm` temporarily switches it to raw,
non-blocking mode and restores settings/flags on exit. UTF-8 input is decoded
and sent through libvterm; Enter, Backspace, Tab, arrows, Home/End,
Insert/Delete, Page Up/Down and F1–F4 have explicit mappings. Unknown complete
escape sequences are forwarded as bytes after at most 40 ms. Ctrl+] always
exits. Ctrl+C exits by default; with `--ctrl-c-to-pty` it is forwarded.

The child is created with `forkpty()`. On exit the master is closed and the
foreground group/child receives SIGHUP; after a bounded wait it is killed if
necessary. Child normal exit status is returned; a handled signal becomes
`128 + signal`, Ctrl+] gives 130, and internal/render/USB failures give 2.
SIGWINCH reapplies the existing PTY winsize; it does not change the selected
font grid.

The presenter renders at most one frame every 50 ms (20 fps ceiling), compares
8×16 terminal tiles with its last successful canvas, and sends changed regions
when cheaper; first frame and costly changes are full frames. Pending frames
are coalesced. FastLZ has no fallback after transport failure. There is no
scrollback user interface; scroll within the visible current screen is
libvterm-owned.

## Practical runs

Safe no-device preview:

```sh
weactterm --dry-run --command "printf 'hello\\n'" --font-size large
```

Run a finite command on verified FS35:

```sh
weactterm --run --device /dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00 \
  --command 'printf "ready\\n"; sleep 2' --font-size normal
```

Interactive session with host diagnostics:

```sh
weactterm --run --device /dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00 \
  --perf-stats --compression off
```

`--brightness 20 --brightness-fade-ms 500` is an explicit hardware setting;
use it only when changing backlight is intended. It is not required for normal
operation.

## Failure diagnosis and limits

`init_status=...` on stderr identifies managed setup; Product/guard failures,
ambiguous selection, busy ports and orientation/transport faults stop without
unsafe fallback. On a disconnect or presenter transfer error, the program ends
rather than reconnecting/replaying a partial frame. `--perf-stats` helps
separate PTY, rasterisation and host transfer work, but does not measure panel
latency. Check `weact-list --verbose`, Product, by-id path, permissions, and
visible owners before retrying. FS35 built-in demo after power-on is handled by
the terminal's full managed init; it clears the screen. Do not run terminal and
`weact-cli` concurrently on one port.
