# Установка и сборка

[English](../en/INSTALL.md) | [Русский](INSTALL.md) | [CLI](CLI.md) | [Оборудование](HARDWARE.md)

## Поддерживаемый релиз и платформа

Готовый релиз — **WeAct35 1.0.0 для Debian ARM64**. Аппаратно и системно
подтверждена Orange Pi 3 LTS с Debian 13 ARM64. Для других архитектур готовые
пакеты и аппаратная квалификация не заявляются.

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
sha256sum -c SHA256SUMS
sudo apt install ./libweact1_1.0.0_arm64.deb ./libweact-dev_1.0.0_arm64.deb ./weactterm_1.0.0_arm64.deb
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

Ожидаются версия `1.0.0`, архитектура `arm64` и pkg-config `1.0.0`.
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
