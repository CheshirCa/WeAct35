# Practical examples

[English](EXAMPLES.md) | [Русский](../ru/EXAMPLES.md) | [Install](INSTALL.md) | [CLI](CLI.md) | [Terminal](TERMINAL.md) | [C API](API.md)

Replace `FS35_PATH` and `FS096_PATH` with your own stable by-id path from
`weact-list --verbose`; they are shell variables, not device names or serials.
Hardware commands below change a display. Before each one confirm Product,
permissions and absence of a visible owner. Do not use
`--allow-unknown-occupancy` unless the intended display is already confirmed
and the scan is only inconclusive because of foreign-process permissions.

## 1. Discover first

Purpose: identify models without opening a serial port.

```sh
weact-list --verbose
FS35_PATH=/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00
FS096_PATH=/dev/serial/by-id/usb-WeAct_Studio_Display_FS_0.96_Inch_YOUR_ID-if00
```

Expected result: FS35 reports Product `Display FS V1`; FS096 reports
`Display FS 0.96 Inch`. Both share `1a86:fe0c`; Product distinguishes them.
The shown paths are examples only—copy the actual path printed on your system.

## 2. Prepare FS35 after power-on

Purpose: stop the changing built-in display state and establish the verified
landscape baseline before partial code updates.

```sh
weact-cli /p:"$FS35_PATH" /init
```

Expected result: exit 0 and a black FS35. `/init` is FS35-only, selects 02,
validates it and clears the whole screen. It changes all FS35 pixels, so do
not use it between regional updates. It does not change brightness or reset.

## 3. Text, clear, and image

Purpose: normal fullscreen output on either supported model.

```sh
weact-cli /p:"$FS35_PATH" /center 'Hello from FS35'
weact-cli /p:"$FS096_PATH" /c:green /f:'DejaVu Sans:12' 'Hello from FS096'
weact-cli /p:"$FS35_PATH" /CLS
weact-cli /p:"$FS35_PATH" /image:/absolute/path/to/logo.png:0 /quality:normal
```

Text uses UTF-8, `/c` chooses a documented colour, `/f` requests font and
size, `/CLS` renders a black clear frame, and images accept PNG/JPEG/BMP/first
GIF frame. These are fullscreen operations except text overlay (`/x` or `/y`),
which is documented in [CLI](CLI.md); image placement is not `/x`/`/y`.

## 4. Fullscreen QR and barcodes

Purpose: centred white field with black modules/bars and integer scaling.

```sh
weact-cli /p:"$FS35_PATH" '/qr:https://example.com'
weact-cli /p:"$FS096_PATH" '/barcode:code128:ABC123'
weact-cli /p:"$FS096_PATH" '/barcode:ean8:1234567'
weact-cli /p:"$FS35_PATH" '/barcode:code39:HELLO-35'
```

QR input is strict UTF-8. Code 128 is ASCII; EAN-8 accepts seven data digits
or eight checksum-validated digits; basic Code 39 is uppercase and does not
accept `*`. The CLI rejects a symbol that does not fit. These examples show
visual output only; external-scanner readability is not claimed.

## 5. Two retained FS35 regions

Purpose: display independent codes without clearing the first one.

```sh
weact-cli /p:"$FS35_PATH" /init
weact-cli /p:"$FS35_PATH" '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
weact-cli /p:"$FS35_PATH" '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
```

The rectangles do not overlap. Each becomes a white code field and only that
rectangle is transmitted; pixels outside are neither read nor sent. All four
`/x /y /w /h` values are mandatory, must fit FS35 480x320, and quiet zones
must fit. Do not add those parameters to FS096: region mode is rejected before
opening its port.

## 6. FS096 fullscreen-only code

Purpose: use the small display without unsupported partial geometry.

```sh
weact-cli /p:"$FS096_PATH" '/qr:FS096-QR-TEST'
weact-cli /p:"$FS096_PATH" '/barcode:ean13:590123412345'
```

Do not add `/w` or `/h`. Fullscreen FS096 code requests landscape before its
bitmap; true overlays preserve state. A no-fit result is safer than clipping.

## 7. Run the FS35 terminal

Purpose: show a controlled command or an interactive shell on FS35.

```sh
weactterm --dry-run --command "printf 'preview\\n'" --font-size large
weactterm --run --device "$FS35_PATH" --command 'uname -a; sleep 2'
```

Dry-run opens no display. `--run` performs a black full managed init, then
uses FS35 only. Ctrl+] exits; Ctrl+C exits unless `--ctrl-c-to-pty` is given.
See [Terminal](TERMINAL.md) for grid sizes, input, compression, brightness,
and lifecycle limits.

## 8. Minimal C program and error handling

Purpose: create a canvas without USB, then compile against the installed API.

```c
#include <stdio.h>
#include <weact/weact_extended.h>
#include <weact/weact_graphics.h>

static int check(wx_status_t s, const char *what) {
    if (s == WX_OK) return 0;
    fprintf(stderr, "%s failed: wx_status_t=%d\n", what, (int)s);
    return 1;
}
int main(void) {
    wx_surface_t *s = NULL;
    wx_status_t st = wx_surface_create(160, 80, 0, WX_LEGACY_BRG565_BE, NULL, &s);
    if (check(st, "surface create")) return 1;
    st = wx_surface_fill_rgb(s, 0, 0, 160, 80, 0, 0, 0);
    if (st == WX_OK) st = wx_surface_set_rgb(s, 4, 4, 255, 255, 255);
    wx_surface_destroy(s);
    return check(st, "surface drawing");
}
```

```sh
cc -std=c11 -Wall -Wextra example.c $(pkg-config --cflags --libs weact) -o example
```

This program has no device I/O. `wx_status_t` is the library error mechanism;
there is no universal detailed error accessor. For a managed FS35 hardware
connection, inspect `wx_init_report_t` after `wx_device_init()`, use the
guarded POSIX connector, and close/destroy resources as described in
[C API](API.md). There is no public C QR/Barcode encoder in 1.0.0; use CLI or
render RGB data yourself then use the documented surface/region APIs.

## 9. Quick diagnosis

```sh
weact-list --verbose
weact-cli /?
weactterm --help
pkg-config --modversion weact
```

For a failed hardware operation, preserve stderr: it carries parse, guard,
initialisation or transport detail. Never retry a partial transfer blindly or
run `weact-cli` and `weactterm` at the same time on one port.

| Capability | FS35 | FS096 |
|---|---|---|
| `/init` | Yes; full black clear | Rejected |
| Text/images/fullscreen codes | Yes | Yes |
| Code region `/x /y /w /h` | Yes | Rejected |
| `weactterm` | Yes | No |
| Native pixels | RGB565 LE, 480x320 | legacy BRG565/MSB, 160x80 |
