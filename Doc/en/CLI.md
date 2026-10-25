# CLI and device discovery

[English](CLI.md) | [Русский](../ru/CLI.md) | [Install](INSTALL.md) | [Hardware](HARDWARE.md)

`weact-cli` changes a selected display. Run `weact-cli /?` for the local
production usage text. `weact-list` is read-only and should be the first tool
used with a newly connected display.

## Exit status

| Status | Meaning |
|---:|---|
| 0 | Help, successful discovery, or successful requested operation |
| 1 | Identification, input, rendering, connection, transmission, or cleanup failure |
| 2 | Invalid `weact-cli` arguments; `weact-list` invalid arguments |
| 128 + signal | A CLI hardware operation was stopped by a handled signal |

Not every error has a unique exit code. Read stderr: invalid code data,
geometry, guard, and transport failures are described there.

## `weact-list`

```text
weact-list [--verbose|--help]
```

It reads sysfs metadata and lists FS35, FS096, and unknown `1a86:fe0c` USB
devices. It never opens a serial node or sends display commands. `--verbose`
adds USB, interface, and driver metadata. Exit 0 also means that no devices
were found; exit 1 means an incomplete scan.

```sh
weact-list
weact-list --verbose
```

Choose the shown `/dev/serial/by-id/...` path for later commands.

## Common selection and safety options

All one-slash option names are case-insensitive; their one-dash equivalents
are accepted. Long options below use two dashes.

| Option | Syntax | Meaning |
|---|---|---|
| Port | `/p:N` | Select `/dev/ttyACMN`, where `N` is 0–999999. |
| Port path | `/p:/dev/ttyACM0` or `/p:/dev/serial/by-id/...` | Select an absolute `/dev/...` path. A port is mandatory for an operation. |
| USB serial | `--serial SERIAL` | Require the matching USB serial, if the device exposes one. |
| Verbose | `/v` | Print model/canvas diagnostic information to stderr. |
| Guard override | `--allow-unknown-occupancy` | Allow only an inconclusive occupancy result after identity checks; never bypasses known busy state. |
| Help | `/?`, `/h`, `--help` | Print help without device access. |

The guard verifies Product, VID:PID, manufacturer, CDC ACM interface and
exclusive access. A failure prints `Guarded connection failed; no fallback`.

## Operation compatibility

| Operation | FS35 | FS096 | Screen effect |
|---|---|---|---|
| `/init` | Yes | Rejected | Whole FS35 becomes black |
| Text, `/CLS`, file, image | Yes | Yes | Full frame, except text overlay |
| Text overlay `/x` or `/y` | Yes | Yes | Only solid glyph pixels are sent |
| Scroll | Yes | Yes | Animated full/partial text updates; clears after one pass |
| `--scroll-compression fastlz` | Yes | Rejected | FS35 scrolling text only |
| Fullscreen QR/Barcode | Yes | Yes | Whole display code frame |
| QR/Barcode region `/x /y /w /h` | Yes | Rejected before opening | Only the FS35 rectangle is sent |

## FS35 initialization

```text
weact-cli /p:PATH /init
```

`/init` has no combinable content or styling options. It is FS35-only, sets
orientation 02, validates it, and performs a black full-screen clear. It does
not set brightness, reset, or free the display. Use it after FS35 power-on,
then add regional codes. FS35 regional preserve accepts readback landscape 02
or 03; portrait 00/01 and unknown values are rejected.

## QR and barcodes

```text
weact-cli /p:PATH /qr:DATA [/center] [/v] [--serial SERIAL] [--allow-unknown-occupancy]
weact-cli /p:PATH /barcode:FORMAT:DATA [/center] [/v] [--serial SERIAL] [--allow-unknown-occupancy]
```

Only one symbol is allowed. Quote an argument containing shell-special bytes
or colons; all text after the second barcode colon remains data. Code modes
cannot be combined with text, `/file`, `/image`, `/CLS`, colour, font,
quality, scrolling, or scroll options.

| Kind | Input rule |
|---|---|
| QR | Strict UTF-8, ECI 26, error correction M; input limit 2953 bytes |
| `code128` | ASCII |
| `code39` | Basic uppercase Code 39 alphabet: `0-9 A-Z space - . $ / + %`; no `*` |
| `ean8` | 7 data digits or 8 digits with valid checksum |
| `ean13` | 12 data digits or 13 digits with valid checksum |

Fullscreen rendering uses a white code field and black modules, centered by
default; `/center` is accepted for compatibility. Scaling is integer-only and
quiet zones are retained. The CLI rejects a symbol that does not fit rather
than clipping it. On FS096, QR is limited to version 13 at one-pixel modules;
EAN uses one-pixel modules. Scanner readability has not been claimed.

### FS35 region codes

```text
weact-cli /p:PATH /qr:DATA /x:X /y:Y /w:W /h:H
weact-cli /p:PATH /barcode:FORMAT:DATA /x:X /y:Y /w:W /h:H
```

All four values are mandatory. `X` and `Y` accept 0–65535, `W` and `H` accept
1–65535, then the selected rectangle must be non-empty and wholly inside
FS35 480×320. `/w` and `/h` are code-only. Supplying only part of the quartet
is an argument error before device access. FS096 region mode is rejected before
opening its port.

The rectangle becomes white with a black symbol; its quiet zone is inside that
rectangle. Only that rectangle is transmitted, and unknown LCD pixels outside
it are not read. Two independent regions:

```sh
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 /init
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
```

## Text, files, and stdin

```text
weact-cli /p:PATH [OPTIONS] [text ...]
printf 'Hello\n' | weact-cli /p:PATH
weact-cli /p:PATH /file:/absolute/path/to/text.txt
```

Input precedence is `/CLS`, image, file, literal text, then redirected stdin.
File and stdin text must be usable UTF-8; a file must be a regular file. Text
input is bounded, empty input fails, and redirected stdin has a bounded wait.
Literal arguments are joined with spaces. `\n` becomes a line break, CRLF is
normalised, tabs become spaces, and a UTF-8 BOM is removed.

| Option | Accepted value | Effect |
|---|---|---|
| `/c:COLOR` | `red`, `green`, `blue`, `white`, `black`, `yellow`, `cyan`, `magenta` | Text colour; default white |
| `/f:FONT[:SIZE]` | Fontconfig name; size clamps to 6–32 px | Select font; default request is Arial with DejaVu Sans fallback |
| `/center` | none | Centre text |
| `/x:X`, `/y:Y` | each 0–65535 | Text position; either activates sparse glyph overlay |
| `/CLS` | none | Render/send a black clear frame |
| `/file:PATH` | bounded regular file path | Read UTF-8 text from file |

For ordinary text, an `x` or `y` outside the selected display is rejected.
Overlay sends only solid glyph pixels and preserves other LCD pixels. Overlay
cannot be combined with image or scrolling because the unknown background must
not be erased.

## Images and conversion

```text
weact-cli /p:PATH /image:PATH[:MODE[:WxH]] [/quality:VALUE]
```

PNG, JPEG, BMP, and the first GIF frame are accepted. The image parser applies
bounds before decoding; alpha is composed over black. `MODE` is numeric:
`0` fits while preserving aspect ratio (default), `1` places the image at the
top-left, and `2` centres it. With `MODE` 1 or 2, the optional `WxH` requests
a scaled size; the result is bounded by the display. Each dimension is 1–4096.
If a path with colons exists as a regular file, it takes priority over suffix
parsing.

`/quality:fast|normal|high|bw|bwfast|bwhigh|0|1|2` selects conversion:
0/fast, 1/normal (default), 2/high, and black/white forms. Image output is a
full frame; `/x` and `/y` are not image placement controls.

## Scrolling

```text
weact-cli /p:PATH /s:SPEED[:u|d] [text]
```

`SPEED` is pixels per second; finite values at or below zero become 10, and
values above 100 clamp to 100. Direction defaults up; `u`/`up` and `d`/`down`
are accepted. Scrolling makes one pass and then clears. `--scroll-stats` writes
host-side measurements to stderr. `--scroll-compression off|fastlz` is valid
only for FS35 scrolling text without overlay, `/CLS`, or image; default is
`off`. It is not a generic display compression switch.

## Common errors

Argument errors return 2 before device work. For content commands the model is
identified before canvas transfer; invalid code geometry/encoding fails before
graphics transfer. A transport failure stops without unsafe retry. Unsupported
names such as `/brightness`, `/orientation`, `/sensor`, and `/fastlz` are not
public `weact-cli` options.
