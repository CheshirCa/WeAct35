# Практические примеры

[English](../en/EXAMPLES.md) | [Русский](EXAMPLES.md) | [Установка](INSTALL.md) | [CLI](CLI.md) | [Терминал](TERMINAL.md) | [C API](API.md)

Замените `FS35_PATH` и `FS096_PATH` на свой stable by-id path из
`weact-list --verbose`; это shell variables, а не device names или serials.
Команды hardware ниже меняют дисплей. Перед каждой подтвердите Product, права
и отсутствие visible owner. Не используйте `--allow-unknown-occupancy`, если
device не подтверждён или scan не является лишь inconclusive из-за
foreign-process permissions.

## 1. Сначала обнаружение

Назначение: определить модели без открытия serial port.

```sh
weact-list --verbose
FS35_PATH=/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00
FS096_PATH=/dev/serial/by-id/usb-WeAct_Studio_Display_FS_0.96_Inch_YOUR_ID-if00
```

Ожидание: FS35 показывает Product `Display FS V1`, FS096 —
`Display FS 0.96 Inch`. У обоих `1a86:fe0c`; модель различает Product.
Пути — примеры: скопируйте фактический path из своей системы.

## 2. Подготовка FS35 после power-on

Назначение: остановить changing built-in state и задать verified landscape
baseline до partial code updates.

```sh
weact-cli /p:"$FS35_PATH" /init
```

Ожидание: exit 0 и чёрный FS35. `/init` действует только на FS35, выбирает 02,
проверяет его и clear всего screen. Она меняет все FS35 pixels, поэтому не
вызывайте её между regional updates. Brightness/reset не меняются.

## 3. Text, clear и image

Назначение: обычный fullscreen output на обеих моделях.

```sh
weact-cli /p:"$FS35_PATH" /center 'Hello from FS35'
weact-cli /p:"$FS096_PATH" /c:green /f:'DejaVu Sans:12' 'Hello from FS096'
weact-cli /p:"$FS35_PATH" /CLS
weact-cli /p:"$FS35_PATH" /image:/absolute/path/to/logo.png:0 /quality:normal
```

Text UTF-8, `/c` выбирает documented colour, `/f` — font/size, `/CLS` рисует
black clear frame, image принимает PNG/JPEG/BMP/first GIF frame. Это fullscreen
operations, кроме text overlay (`/x` или `/y`) из [CLI](CLI.md); `/x`/`/y` не
являются image placement.

## 4. Fullscreen QR и Barcode

Назначение: centred white field с black modules/bars и integer scaling.

```sh
weact-cli /p:"$FS35_PATH" '/qr:https://example.com'
weact-cli /p:"$FS096_PATH" '/barcode:code128:ABC123'
weact-cli /p:"$FS096_PATH" '/barcode:ean8:1234567'
weact-cli /p:"$FS35_PATH" '/barcode:code39:HELLO-35'
```

QR — strict UTF-8. Code 128 — ASCII; EAN-8 — семь data digits или восемь с
valid checksum; basic Code 39 — uppercase без `*`. No-fit отвергается. Это
visual output, external-scanner readability не заявляется.

## 5. Два сохраняемых региона FS35

Назначение: вывести независимые codes без стирания первого.

```sh
weact-cli /p:"$FS35_PATH" /init
weact-cli /p:"$FS35_PATH" '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
weact-cli /p:"$FS35_PATH" '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
```

Rectangles не пересекаются. Каждый получает white code field, передаётся только
он; outside pixels не читаются и не передаются. Все `/x /y /w /h` обязательны,
должны поместиться в FS35 480x320 вместе с quiet zones. На FS096 параметры
region не добавляйте: режим отвергается до открытия port.

## 6. Fullscreen code для FS096

Назначение: использовать small display без unsupported partial geometry.

```sh
weact-cli /p:"$FS096_PATH" '/qr:FS096-QR-TEST'
weact-cli /p:"$FS096_PATH" '/barcode:ean13:590123412345'
```

`/w` и `/h` не добавляйте. FS096 для code mode уже должен быть landscape; нет
verified preserve-orientation readback. No-fit безопаснее clipping.

## 7. Запуск FS35 terminal

Назначение: controlled command либо interactive shell на FS35.

```sh
weactterm --dry-run --command "printf 'preview\\n'" --font-size large
weactterm --run --device "$FS35_PATH" --command 'uname -a; sleep 2'
```

Dry-run не открывает display. `--run` делает black full managed init и работает
только с FS35. Ctrl+] exits; Ctrl+C exits, если нет `--ctrl-c-to-pty`. Grid,
input, compression, brightness и lifecycle limits — в [Terminal](TERMINAL.md).

## 8. Минимальная C-программа и обработка ошибок

Назначение: создать canvas без USB и скомпилировать с installed API.

```c
#include <stdio.h>
#include <weact/weact_extended.h>
#include <weact/weact_graphics.h>
static int check(wx_status_t s, const char *what) {
    if (s == WX_OK) return 0;
    fprintf(stderr, "%s failed: wx_status_t=%d\n", what, (int)s); return 1;
}
int main(void) {
    wx_surface_t *s = NULL;
    wx_status_t st = wx_surface_create(160, 80, 0, WX_LEGACY_BRG565_BE, NULL, &s);
    if (check(st, "surface create")) return 1;
    st = wx_surface_fill_rgb(s, 0, 0, 160, 80, 0, 0, 0);
    if (st == WX_OK) st = wx_surface_set_rgb(s, 4, 4, 255, 255, 255);
    wx_surface_destroy(s); return check(st, "surface drawing");
}
```

```sh
cc -std=c11 -Wall -Wextra example.c $(pkg-config --cflags --libs weact) -o example
```

Программа не делает device I/O. `wx_status_t` — механизм library errors; общего
detailed accessor нет. Для managed FS35 inspect `wx_init_report_t` после
`wx_device_init()`, используйте guarded POSIX connector и close/destroy как в
[C API](API.md). Public C QR/Barcode encoder 1.0.0 не предоставляет: примените
CLI либо render RGB сами и используйте documented surface/region API.

## 9. Быстрая диагностика

```sh
weact-list --verbose
weact-cli /?
weactterm --help
pkg-config --modversion weact
```

При failed hardware operation сохраните stderr: там parser/guard/init/transport
detail. Partial transfer не повторяйте blindly и не запускайте `weact-cli` и
`weactterm` одновременно на одном port.

| Capability | FS35 | FS096 |
|---|---|---|
| `/init` | Yes; full black clear | Rejected |
| Text/images/fullscreen codes | Yes | Yes |
| Code region `/x /y /w /h` | Yes | Rejected |
| `weactterm` | Yes | No |
| Native pixels | RGB565 LE, 480x320 | legacy BRG565/MSB, 160x80 |
