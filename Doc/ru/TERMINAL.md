# Руководство WeActTerm

[English](../en/TERMINAL.md) | [Русский](TERMINAL.md) | [Установка](INSTALL.md) | [CLI](CLI.md) | [Оборудование](HARDWARE.md)

`weactterm` — foreground presenter терминала только для FS35. Он запускает
shell либо одну shell-команду в POSIX PTY, libvterm владеет экраном терминала,
а FreeType растеризует его в canvas FS35 480x320. Это удобно для status console,
interactive maintenance shell и контролируемого вывода команды. Это не замена
полноценного desktop terminal: нет UI scrollback, выбора font во время работы,
mouse protocol и обещания полной terminal-emulator совместимости сверх
модели экрана libvterm и описанного ниже input.

## Область, дисплей и зависимости

Поддержан только FS35 (`Display FS V1`, 480x320 RGB565 little-endian). FS096
отвергается: его 160x80 legacy pixel format и geometry не предназначены для
терминала. Используются libvterm 0.3.3, FreeType, DejaVu Sans Mono, PTY и
guarded FS35 connection из `libweact`. В hardware mode выполняется обычный
managed FS35 init: orientation 02 задаётся/подтверждается, а весь экран
очищается чёрным до передачи terminal frames.

## Синтаксис и режимы

```text
weactterm [--dry-run | --run] [--allow-unknown-occupancy]
          [--command 'shell command'] [--ctrl-c-to-pty] [--perf-stats]
          [--device /dev/...] [--serial SERIAL]
          [--font-size tiny|small|normal|large|xlarge]
          [--compression off|fastlz]
          [--brightness 0..50] [--brightness-fade-ms 0..65535]
```

Без аргументов запускается конечная memory-only PTY demonstration и USB не
открывается. `--dry-run` явно выбирает тот же режим. Только `--run` открывает
FS35. Два mode option взаимоисключающие. `--help` завершится 0; неверные или
повторные option и fade без brightness завершаются 2.

| Option | Значение и пределы |
|---|---|
| `--run` | Guarded hardware mode FS35; нужен один matching device. |
| `--dry-run` | Memory-only mode для проверки PTY/font/terminal rendering. |
| `--command CMD` | Child выполняет `/bin/sh -c CMD`. Без неё dry-run использует finite built-in demo, а `--run` — interactive `$SHELL` с fallback `/bin/sh`. |
| `--device PATH` | Absolute CDC ACM/by-id path короче 4096 bytes; лучше путь из `weact-list --verbose`. |
| `--serial SERIAL` | Требует USB serial 1–63 bytes и устраняет неоднозначность. |
| `--allow-unknown-occupancy` | Узкое разрешение только foreign-process permission failure после identity check; busy/wrong Product/namespace/scanner/lock failure не проходят. |
| `--font-size NAME` | `tiny`, `small`, `normal` (default), `large`, `xlarge`; grids ниже. |
| `--compression off|fastlz` | Default `off`; FastLZ — явный FS35 presenter mode, не generic compression/recovery. |
| `--brightness N` | Явное 0–50% backlight request после init; omission brightness не меняет. |
| `--brightness-fade-ms N` | Требует brightness; 0–65535, protocol ограничивает effective fade 5000 ms. |
| `--ctrl-c-to-pty` | Forward Ctrl+C в PTY, иначе presenter exits. |
| `--perf-stats` | Пишет host timing/counters в stderr на exit; LCD commit time не измеряется. |

## Безопасный выбор hardware

Сначала list без открытия tty:

```sh
weact-list --verbose
```

Подтвердите Product `Display FS V1`, права, stable `/dev/serial/by-id/` и
отсутствие другого владельца. Обе WeAct модели имеют VID:PID `1a86:fe0c`,
поэтому одного VID:PID недостаточно. Guarded run auto-select только ровно один
FS35; при нескольких используйте `--device` или `--serial`. Не добавляйте
`--allow-unknown-occupancy` для подавления ошибки: оно уместно только после
проверки своего device и отсутствия visible owner.

```sh
weactterm --run --device /dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00
```

`YOUR_ID` — нейтральный placeholder; скопируйте свой путь из `weact-list`.

## Сетка, цвета и rendering

Используется фиксированный DejaVu Sans Mono. libvterm получает UTF-8,
включая Cyrillic, поддерживаемый проверенным font; child получает `TERM=xterm`
и `LC_ALL=C.UTF-8`. PTY winsize соответствует выбранной grid.

| Size | Cell (pixels) | Grid (columns × rows) |
|---|---:|---:|
| `tiny` | 5×10 | 96×32 |
| `small` | 6×12 | 80×26 |
| `normal` | 8×16 | 60×20 |
| `large` | 12×24 | 40×13 |
| `xlarge` | 16×32 | 30×10 |

libvterm владеет cursor/current screen/colours/supported control sequences;
FreeType blended glyphs на RGB565 canvas. Default foreground `(224,224,224)`,
background black. Не обещаются colour calibration, complex Unicode shaping,
arbitrary fonts, resize на другую grid и visual LCD result только по host TX.

## Input, PTY, signals и lifecycle

Если stdin — terminal, программа временно включает raw non-blocking и на
выходе восстанавливает settings/flags. UTF-8 input декодируется и идёт через
libvterm; явно mapped Enter, Backspace, Tab, arrows, Home/End,
Insert/Delete, Page Up/Down, F1–F4. Unknown complete escape forwards bytes
после максимум 40 ms. Ctrl+] всегда exits. Ctrl+C по default exits, с
`--ctrl-c-to-pty` forward в PTY.

Child создаётся через `forkpty()`. При exit закрывается master, foreground
group/child получает SIGHUP, затем после bounded wait при необходимости kill.
Normal child exit status возвращается; handled signal — `128 + signal`,
Ctrl+] — 130, internal/render/USB failure — 2. SIGWINCH повторно задаёт тот
же PTY winsize, но font grid не меняет.

Presenter максимум один frame каждые 50 ms (20 fps), сравнивает terminal
tiles 8×16 с последним successful canvas и отправляет changed regions, если
они дешевле; первый/costly change — full frame. Pending frames coalesce.
После transport failure FastLZ fallback нет. UI scrollback нет; scroll
видимого current screen принадлежит libvterm.

## Практические запуски

No-device preview:

```sh
weactterm --dry-run --command "printf 'hello\\n'" --font-size large
```

Finite command на verified FS35:

```sh
weactterm --run --device /dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00 \
  --command 'printf "ready\\n"; sleep 2' --font-size normal
```

Interactive session с host diagnostics:

```sh
weactterm --run --device /dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_YOUR_ID-if00 \
  --perf-stats --compression off
```

`--brightness 20 --brightness-fade-ms 500` — явное hardware setting; оно не
нужно для обычной работы и применяется только когда backlight менять нужно.

## Диагностика и ограничения

`init_status=...` в stderr относится к managed setup; Product/guard failure,
ambiguous selection, busy port, orientation/transport faults прекращают работу
без unsafe fallback. При disconnect/presenter transfer error нет reconnect или
replay partial frame. `--perf-stats` разделяет PTY/rasterisation/host transfer,
но не LCD latency. До повторной попытки проверьте `weact-list --verbose`,
Product, by-id path, permissions, visible owners. Built-in demo FS35 после
power-on устраняется full managed init, который clear screen. Не запускайте
terminal и `weact-cli` одновременно на одном port.
