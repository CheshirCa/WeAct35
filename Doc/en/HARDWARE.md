# Hardware and safe operation

[English](HARDWARE.md) | [Русский](../ru/HARDWARE.md) | [Install](INSTALL.md) | [CLI](CLI.md)

## Supported displays

| Property | FS35 | FS096 |
|---|---:|---:|
| Exact Product | `Display FS V1` | `Display FS 0.96 Inch` |
| USB VID:PID | `1a86:fe0c` | `1a86:fe0c` |
| Logical landscape geometry | 480×320 | 160×80 |
| Library pixel format | RGB565 little-endian | legacy BRG565/MSB |
| Regional QR/Barcode | Yes | No |
| `weactterm` | Yes | No |

The different pixel packing is significant: FS35 RGB565 LE and FS096 legacy
BRG565/MSB are not interchangeable. Let the library select the model; do not
send a framebuffer intended for one display to the other.

## Identification and ports

Use `weact-list --verbose` first. It reads sysfs and reports recognized FS35,
FS096, and unknown devices with the same VID:PID. `weact-cli` accepts either
`/p:N` for `/dev/ttyACMN` or a `/p:/dev/serial/by-id/...` path. Prefer by-id.

Before writing, confirm all of the following:

1. The Product string matches the expected display.
2. The selected path resolves to the intended CDC ACM device.
3. Your user has read/write access.
4. No other program visibly owns the port.

The guarded connection verifies USB identity, CDC ACM identity, exclusive
ownership, and occupancy. `--allow-unknown-occupancy` is only appropriate when
the device itself has already been confirmed and no visible owner exists; it
does not ignore a known busy state or identity failure.

## FS35 startup and regions

After USB power is removed and restored, FS35 may display a cyclic built-in
demo whose orientation changes. The safe sequence is:

```sh
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 /init
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
```

`/init` is a standalone operation. It sets orientation 02, validates the
reply, and clears the complete FS35 framebuffer black. It does not change
brightness, reset, or free the device. Regional codes then use the preserve
path: FS35 accepts confirmed landscape response 02 or 03, requires 480×320,
and sends only the requested rectangle. Do not issue `/init` between regions.

Regional updates use a white background with a black code and retain integer
module scaling and quiet zones. Unknown LCD content is never read. Offline
tests check exact buffers and packets; hardware evidence is visual, not a
byte-for-byte capture of LCD pixels.

## FS096 behavior

FS096 code rendering is fullscreen at 160×80. It has no verified orientation
readback for preserved regional updates, so a code region is rejected before
the serial port is opened. Code modes preserve orientation and require the
display to already be landscape. Ordinary text/fullscreen paths use the legacy
FS096 setup. FS096 QR is limited by its small display; the CLI reports no-fit
instead of clipping. EAN is limited to one pixel per module.

## Transfer and failure behavior

Large frames are serial transfers, not instantaneous screen changes. Do not
assume measured host transmit success proves a visual result. A failure in
identity, geometry, encoder setup, orientation confirmation, or transport is
fail-stop: the CLI does not use an unsafe fallback or retry partial output.

For FS35 scrolling text, optional FastLZ compression is explicit and only
available with FS35 scrolling. It is not a generic command switch. Scroll
statistics describe host-side work and transfers; they are not a promise of
LCD refresh rate.

## Confirmed scope and limits

Hardware confirmation includes FS35 `/init`, separate regional Code 128 and
QR, and FS096 fullscreen Code 128 and QR. No external scanner result is
claimed. Do not infer untested firmware features, colour characteristics, or
physical-pixel guarantees from this documentation.
