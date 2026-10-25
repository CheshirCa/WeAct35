# WeAct35 1.0.0

[English](README.md) | [Русский](README.ru.md)

WeAct35 — C-библиотека и набор программ командной строки для USB-дисплеев WeAct. Проект предназначен для пользователя Linux, который хочет вывести текст, изображение, QR или штрихкод, и для разработчика C-приложений.

## Платформы и статус релизов

- **AMD64:** Debian 13 x86-64; сборка, пакеты 1.0.0-3 и отдельные аппаратные сценарии FS35/FS096 проверены локально. AMD64 assets подготовлены локально и ожидают добавления в существующий GitHub Release.
- **ARM64:** Orange Pi 3 LTS, Debian 13; проверенная packaged revision — `1.0.0-3`.
- **riscv64:** Orange Pi RV2, Ubuntu 24.04.5 LTS; установленная и аппаратно smoke-проверенная revision — `1.0.0-3` upstream `1.0.0`.

Пакеты архитектур публикуются отдельными Release assets с собственными checksum manifests; в исходное дерево они не коммитятся. Перед загрузкой проверьте наличие assets нужной архитектуры в существующем Release.

## Дисплеи

| Модель | USB Product | Разрешение | Представление пикселей | Назначение |
|---|---|---:|---|---|
| FS35 | `Display FS V1` | 480×320 | RGB565, little-endian | Терминал, текст, изображения, полноэкранные и региональные коды |
| FS096 | `Display FS 0.96 Inch` | 160×80 | legacy BRG565/MSB | Компактный текст, изображения, полноэкранные коды |

Оба устройства — USB CDC ACM с VID:PID `1a86:fe0c`. Программы проверяют точный Product до записи. FS35 поддерживает QR/Barcode в области с сохранением вне неё; FS096 поддерживает коды только на весь экран.

## Состав проекта

- `libweact.so.1` — C-библиотека графики и транспорта; публичный ABI содержит 79 экспортируемых символов.
- `weact-cli` — текст, UTF-8 из файла/stdin, изображения, прокрутка, QR, Code 128, EAN-8, EAN-13 и Code 39.
- `weact-list` — read-only инвентаризация USB, не открывает последовательный порт.
- `weactterm` — FS35-терминал PTY с libvterm и FreeType. По умолчанию это конечная memory-only демонстрация; `--run` включает устройство.

`/init` действует только на FS35: устанавливает проверенный landscape и очищает экран в чёрный. После power-cycle выполните `/init` до региональных кодов FS35. Не вызывайте `/init` между областями: она их сотрёт.

## Быстрый старт Debian ARM64

Проверьте и установите три неотладочных пакета:

```sh
cd Release/arm64
sha256sum -c SHA256SUMS-arm64.txt
sudo apt install ./libweact1_1.0.0-3_arm64.deb ./libweact-dev_1.0.0-3_arm64.deb ./weactterm_1.0.0-3_arm64.deb
```

Найдите устройства без открытия порта:

```sh
weact-list --verbose
```

Предпочитайте путь `/dev/serial/by-id/`. Следующие команды обращаются к устройству; запускайте их после проверки дисплея, прав доступа и отсутствия другого владельца:

```sh
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 /init
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
```

Две области FS35 не пересекаются. Белый фон кода заменяет только свой прямоугольник; пиксели вне области не читаются и не передаются.

## Сборка из исходников

```sh
build=$(mktemp -d)
make BUILD_DIR="$build/obj" BIN_DIR="$build/bin" all
LD_LIBRARY_PATH="$build/obj" "$build/bin/weact-cli" /?
```

Для Debian-сборки используйте `dpkg-buildpackage -b -us -uc` в этом каталоге при установленных build dependencies.

## Документация

- [Установка и сборка](Doc/ru/INSTALL.md)
- [CLI и обнаружение устройств](Doc/ru/CLI.md)
- [Оборудование и безопасная работа](Doc/ru/HARDWARE.md)
- [Справочник C API](Doc/ru/API.md)
- [Руководство терминала](Doc/ru/TERMINAL.md)
- [Практические примеры](Doc/ru/EXAMPLES.md)
- English: [installation](Doc/en/INSTALL.md), [CLI](Doc/en/CLI.md), [hardware](Doc/en/HARDWARE.md), [C API](Doc/en/API.md)
- English: [terminal](Doc/en/TERMINAL.md), [examples](Doc/en/EXAMPLES.md)

Языковые каталоги содержат каноническую пользовательскую документацию.
- [Release notes 1.0.0-3 и каталог пакетов архитектур](Doc/en/RELEASE-1.0.0-3.md)
- [CDC ACM для Orange Pi RV2](Misc/README.ru.md)

После установки пакетов доступны краткие английские man-страницы:
`man weact-cli`, `man weact-list`, `man weactterm` и `man 3 libweact`.
Первые три устанавливает `weactterm`, обзор библиотеки — `libweact-dev`.

## Проверка и ограничения

Сборка AMD64 и offline-регрессия, включая ASan/UBSan, прошли; LSan не
проверялся. На Debian 13 AMD64 пользователь подтвердил очистку FS35, текст, QR
и QR в области; true overlay FS096 сохранил QR под крупным текстом, а первый
fullscreen Code128 после переподключения прошёл в трёх попытках с новым
кабелем. В одной из двух дополнительных попыток со старым кабелем наблюдалось
смещение изображения; это наблюдение, а не установленный дефект ПО.
Подтверждённые сценарии ARM64 и RISC-V приведены в release notes. Внешнее
сканирование и побайтный захват LCD-пикселей не заявляются.

Исходный набор содержит notices Expat/MIT, GPL-3.0-only, LGPL-2.1-or-later и BSD-3-Clause в `LICENSE` и `LICENSES/`. Публичное распространение всё ещё требует подтверждения владельцем прав на GPL-адаптацию.
