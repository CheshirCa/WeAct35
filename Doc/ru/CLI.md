# CLI и обнаружение устройств

[English](../en/CLI.md) | [Русский](CLI.md) | [Установка](INSTALL.md) | [Оборудование](HARDWARE.md)

`weact-cli` изменяет выбранный дисплей. Реальная локальная справка:
`weact-cli /?`. `weact-list` read-only и должен быть первой программой для
нового устройства.

## Exit status

| Код | Значение |
|---:|---|
| 0 | Help, успешное обнаружение или запрошенная операция |
| 1 | Ошибка identity/input/render/connect/transmission/cleanup |
| 2 | Неверные аргументы `weact-cli` или `weact-list` |
| 128 + signal | Аппаратная операция CLI остановлена обработанным сигналом |

Не у каждой ошибки отдельный код: читайте stderr.

## `weact-list`

```text
weact-list [--verbose|--help]
```

Программа читает sysfs и показывает FS35, FS096 и UNKNOWN `1a86:fe0c`; serial
порт не открывается, display-команды не отправляются. `--verbose` добавляет
USB/interface/driver metadata. Код 0 также означает «устройств нет», код 1 —
неполный scan.

```sh
weact-list
weact-list --verbose
```

Для следующих команд выбирайте показанный `/dev/serial/by-id/...`.

## Общие параметры и защита

Однослешевые имена case-insensitive; допускается форма с одним `-`. Long
options используют два `--`.

| Параметр | Синтаксис | Значение |
|---|---|---|
| Порт | `/p:N` | `/dev/ttyACMN`, `N` от 0 до 999999 |
| Путь | `/p:/dev/ttyACM0` или `/p:/dev/serial/by-id/...` | Абсолютный путь `/dev/...`; обязателен для операции |
| USB serial | `--serial SERIAL` | Требует совпадения USB serial, если он есть |
| Диагностика | `/v` | Печатает model/canvas в stderr |
| Occupancy | `--allow-unknown-occupancy` | Принимает только неокончательное occupancy после identity checks; не игнорирует known busy |
| Help | `/?`, `/h`, `--help` | Справка без доступа к устройству |

Guard проверяет Product, VID:PID, manufacturer, CDC ACM interface и эксклюзив.
При отказе выводится `Guarded connection failed; no fallback`.

## Совместимость операций

| Операция | FS35 | FS096 | Влияние на экран |
|---|---|---|---|
| `/init` | Да | Отклоняется | Весь FS35 чёрный |
| Text, `/CLS`, file, image | Да | Да | Полный кадр, кроме text overlay |
| Text overlay `/x` или `/y` | Да | Да | Передаются только solid glyph pixels |
| Scroll | Да | Да | Анимированные text updates, затем clear |
| `--scroll-compression fastlz` | Да | Отклоняется | Только scrolling text FS35 |
| Fullscreen QR/Barcode | Да | Да | Полноэкранный код |
| QR/Barcode region | Да | До open отклоняется | Передаётся только FS35 rectangle |

## `/init` для FS35

```text
weact-cli /p:PATH /init
```

`/init` нельзя сочетать с content/styling. Это FS35-only: orientation 02,
проверка ответа, full-screen black clear. Она не меняет brightness, не делает
Reset/Free. После power-on сначала `/init`, затем регионы. Preserve принимает
FS35 landscape readback 02/03; portrait 00/01 и неизвестные значения
отклоняются.

## QR и штрихкоды

```text
weact-cli /p:PATH /qr:DATA [/center] [/v] [--serial SERIAL] [--allow-unknown-occupancy]
weact-cli /p:PATH /barcode:FORMAT:DATA [/center] [/v] [--serial SERIAL] [--allow-unknown-occupancy]
```

Разрешён один символ. Заключайте данные с shell-special bytes/colon в кавычки;
всё после второго colon barcode остаётся data. Коды нельзя сочетать с text,
`/file`, `/image`, `/CLS`, color/font/quality, scroll и его options.

| Вид | Правило input |
|---|---|
| QR | Строгий UTF-8, ECI 26, ECC M, до 2953 bytes |
| `code128` | ASCII |
| `code39` | `0-9 A-Z space - . $ / + %`, без `*` |
| `ean8` | 7 data digits или 8 с valid checksum |
| `ean13` | 12 data digits или 13 с valid checksum |

Fullscreen — белое поле, чёрные modules, integer-only scaling и quiet zones;
код, не помещающийся, отклоняется без clipping. На FS096 QR ограничен V13 при
1px modules, EAN — 1px/module. Считывание сканером не заявляется.

### Регион FS35

```text
weact-cli /p:PATH /qr:DATA /x:X /y:Y /w:W /h:H
weact-cli /p:PATH /barcode:FORMAT:DATA /x:X /y:Y /w:W /h:H
```

Все четыре обязательны. `X/Y` — 0..65535, `W/H` — 1..65535, после чего
прямоугольник обязан быть непустым и целиком внутри FS35 480×320. `/w`/`/h`
действуют только для кодов. Частичный набор — argument error до доступа к
устройству. FS096 region отклоняется до open.

Фон региона белый, символ чёрный, quiet zone внутри; передаётся только region,
неизвестные пиксели LCD вне него не читаются. Два независимых региона:

```sh
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 /init
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
```

## Текст, файл и stdin

```text
weact-cli /p:PATH [OPTIONS] [text ...]
printf 'Hello\n' | weact-cli /p:PATH
weact-cli /p:PATH /file:/absolute/path/to/text.txt
```

Приоритет: `/CLS`, image, file, literal text, redirected stdin. File/stdin
должны давать пригодный UTF-8; file — regular file. Пустой/слишком большой/
таймаутный input отклоняется. Literal arguments соединяются пробелом; `\n`
становится newline, CRLF нормализуется, tabs становятся spaces, UTF-8 BOM
удаляется.

| Параметр | Допустимо | Действие |
|---|---|---|
| `/c:COLOR` | red, green, blue, white, black, yellow, cyan, magenta | Цвет текста, default white |
| `/f:FONT[:SIZE]` | Fontconfig name, size clamp 6..32 | Font; default request Arial, fallback DejaVu Sans |
| `/center` | — | Центрирование текста |
| `/x:X`, `/y:Y` | 0..65535 | Позиция; любой включает sparse glyph overlay |
| `/CLS` | — | Black clear frame |
| `/file:PATH` | bounded regular file | UTF-8 text из файла |

Обычный text с `x/y` вне выбранного display отклоняется. Overlay отправляет
только solid glyph pixels и сохраняет остальное; с image/scroll он запрещён.

## Изображения и conversion

```text
weact-cli /p:PATH /image:PATH[:MODE[:WxH]] [/quality:VALUE]
```

Поддержаны PNG/JPEG/BMP/первый GIF frame. До decode применяются bounds, alpha
накладывается на black. `MODE`: `0` fit с aspect ratio (default), `1` помещает
изображение в top-left, `2` центрирует. Для MODE 1/2 необязательный `WxH`
задаёт масштабируемый размер, ограниченный экраном; каждая сторона 1..4096.
Существующий regular filename с colon имеет приоритет над suffix parsing.

`/quality:fast|normal|high|bw|bwfast|bwhigh|0|1|2`: 0/fast, 1/normal default,
2/high и black/white формы. Image отправляется full frame; `/x`/`/y` не
задают её положение.

## Прокрутка и ошибки

```text
weact-cli /p:PATH /s:SPEED[:u|d] [text]
```

`SPEED` — pixels/sec; finite `<=0` становится 10, `>100` становится 100.
Default направление up; разрешены `u/up`, `d/down`. Один проход завершается
clear. `--scroll-stats` пишет host measurements в stderr.
`--scroll-compression off|fastlz` работает только для scrolling text FS35 без
overlay, `/CLS`, image; default `off`.

Invalid arguments дают 2 до доступа к устройству. Ошибки code geometry/encoder
останавливают вывод до graphics transfer; transport failure не получает unsafe
retry. `/brightness`, `/orientation`, `/sensor`, `/fastlz` не являются
публичными опциями `weact-cli`.
