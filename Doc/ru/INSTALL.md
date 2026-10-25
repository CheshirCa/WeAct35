# Установка и сборка

[English](../en/INSTALL.md) | [Русский](INSTALL.md) | [CLI](CLI.md) | [Оборудование](HARDWARE.md)

## Поддерживаемый релиз и платформа

WeAct35 1.0.0-3 собран и проверен на Debian 13 `amd64`, Debian 13 `arm64` и
Ubuntu 24.04.5 `riscv64`. AMD64-пакеты установлены, а отдельные аппаратные
сценарии визуально подтверждены на Debian 13. Результаты ARM64 и RISC-V
суммированы в release notes. Проверьте список assets существующего релиза:
локально подготовленные AMD64-пакеты нельзя скачать, пока они туда не добавлены.

| Пакет | Назначение |
|---|---|
| `libweact1` | Runtime `libweact.so.1` |
| `libweact-dev` | Публичные headers, static library, `weact.pc`, C example |
| `weactterm` | `weact-cli`, `weact-list`, `weactterm`, FS35 feature tool |

`-dbgsym` — необязательные пакеты символов отладки.

## Установка ARM64-пакетов

Из верхнего каталога export:

```sh
cd Release/arm64
sha256sum -c SHA256SUMS-arm64.txt
sudo apt install ./libweact1_1.0.0-3_arm64.deb ./libweact-dev_1.0.0-3_arm64.deb ./weactterm_1.0.0-3_arm64.deb
```

`apt` разрешает runtime dependencies: FreeType, GDK-Pixbuf/GLib, libvterm,
`libqrencode4`, fontconfig и DejaVu fonts. Не устанавливайте пакет при
несовпадении checksum, версии или архитектуры.

Проверка установки:

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

Ожидаются Debian package version `1.0.0-3`, архитектура `arm64` и
pkg-config upstream API version `1.0.0`.
`weactterm` устанавливает три program pages section 1; `libweact-dev` —
library overview section 3. Полные двуязычные guides остаются в исходном
каталоге документации.
Основные пути: `/usr/bin/weact-cli`, `/usr/bin/weact-list`,
`/usr/bin/weactterm`, `/usr/lib/aarch64-linux-gnu/libweact.so.1`,
`/usr/include/weact/`, `/usr/lib/aarch64-linux-gnu/pkgconfig/weact.pc`.

Безопасное удаление пакетов:

```sh
sudo apt remove weactterm libweact-dev libweact1
```

Перед подтверждением просмотрите предложенную транзакцию.

## Статус RISC-V RV2

На Orange Pi RV2 с Ubuntu 24.04.5 LTS установлен и аппаратно smoke-проверен набор `riscv64` revision `1.0.0-3` upstream `1.0.0`. Получайте все пять release assets вместе с `SHA256SUMS`, проверяйте их, затем устанавливайте три неотладочных `.deb` через `apt`. Пакеты ARM64 для Debian 13 также имеют revision `1.0.0-3`.

См. [RISC-V release notes](RELEASE-1.0.0-3.md) и [заметку о CDC ACM RV2](../../Misc/README.ru.md).

## Статус сборки из исходников для Debian 13 AMD64

Общее дерево исходников собирается нативно на Debian 13 x86-64 (`amd64`)
без архитектурных изменений кода. Динамическая/статическая библиотека,
`weact-cli`, `weact-list`, `weact-fs35-features` и `weactterm` собраны с
`-Wall -Wextra -Werror -O2 -std=c11` и Debian hardening flags. Терминальные/PTY
и связанные offline runtime-регрессии пройдены. Версия upstream API
остаётся `1.0.0`, SONAME — `libweact.so.1`.

Для полной сборки, включая `weactterm`, нужен `libvterm-dev` с libvterm
строго `0.3.3`; теперь эта зависимость доступна и полная сборка исходников
на AMD64 пройдена. Существующий Debian packaging использует
`Architecture: any` и выбирает на AMD64 `/usr/lib/x86_64-linux-gnu`.
Пять бинарных AMD64-пакетов 1.0.0-3 прошли проверки checksum, lintian, ABI и
dependencies; пользователь установил и проверил `libweact1`, `libweact-dev` и
`weactterm`. На AMD64 визуально подтверждены очистка FS35, текст, QR и вывод
QR в области. На FS096 true overlay сохранил QR при наложении крупного текста;
первый fullscreen Code128 после cold-start прошёл в трёх попытках с новым
USB-кабелем. В одной из двух дополнительных попыток со старым кабелем
наблюдалось смещение изображения; это наблюдение не доказывает дефект ПО.
Остальные аппаратные сценарии не подтверждены. AMD64-пакеты остаются локальными
и ещё должны быть добавлены в assets существующего GitHub-релиза. Используйте
существующие команды сборки ниже; статус ARM64/RISC-V не меняется.

## Поиск USB и права

До любой аппаратной команды:

```sh
weact-list --verbose
```

`weact-list` читает только sysfs. Выбирайте показанный `/dev/serial/by-id/...`:
он устойчив к смене номера tty. Пользователь должен иметь доступ чтения/записи
к выбранному `/dev/ttyACM*`; сначала проверьте это, а не меняйте права вслепую:

```sh
ls -l /dev/ttyACM0
id
```

Изменение групп, прав или device policy требует обычной административной
процедуры дистрибутива. Не обходите проверку владельца и не открывайте
неподтверждённый порт.

## Сборка из export

Нужны GNU make/GCC, pkg-config, `libfreetype-dev`, `libvterm-dev` ровно 0.3.3,
libqrencode development support, GDK-Pixbuf/GLib development support,
fontconfig, debhelper-compat 13 и Python 3 для Debian packaging. Makefile
проверяет FreeType и libvterm 0.3.3.

Собирайте вне export:

```sh
build=$(mktemp -d)
make BUILD_DIR="$build/obj" BIN_DIR="$build/bin" all
LD_LIBRARY_PATH="$build/obj" "$build/bin/weact-cli" /?
```

Результат: `libweact.so.1.0.0`, `libweact.a`, `weact-cli`, `weact-list`,
`weactterm`, `weact-fs35-features` в `$build`. Публичный export намеренно не
содержит test harness.

Debian binary packages собираются, но не устанавливаются, командой:

```sh
dpkg-buildpackage -b -us -uc
```

## Типичные проблемы

| Симптом | Что проверить |
|---|---|
| `pkg-config` не находит `weact` | Установите `libweact-dev`; проверьте конфликтующий `PKG_CONFIG_PATH`. |
| CLI не открывает устройство | `weact-list --verbose`, точный Product, by-id путь, права и владельца порта. |
| FS35 показывает меняющуюся заставку | Однократно выполните standalone `/init`, затем региональные коды. |
| Регион кода отклонён | Нужны FS35 и полный набор `/x /y /w /h`; FS096 — только fullscreen. |
| Source build останавливается на vterm | Требуется libvterm 0.3.3 development package. |

Публичное распространение ожидает подтверждения владельцем прав на
GPL-адаптацию.
