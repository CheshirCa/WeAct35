# WeAct35 1.0.0-3 — release notes

GitHub Release [v1.0.0-3](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3)
содержит native Debian packages для Debian 13 `amd64`, Debian 13 `arm64` и
Orange Pi RV2 / Ubuntu 24.04.5 LTS `riscv64`: по пять пакетов и одному checksum
manifest для каждой архитектуры, всего 18 assets. AMD64-файлы скачаны из
Release и прошли SHA-256. Версии пакетов и исторический тег не изменены.

## Основное

- Каждая полноэкранная сессия FS096 перед bitmap запрашивает landscape, что
  исправляет cold-start splash/orientation. Настоящий text overlay сохраняет
  состояние устройства.
- ABI не изменён: SONAME `libweact.so.1`, 79 экспортов, pkg-config `1.0.0`.
- Подтверждённые пользователем ARM64 results: FS096 cold-start QR и Code 128,
  FS096 true overlay, FS35 text, WeActTerm и Midnight Commander.
- Подтверждённые пользователем RISC-V results: FS35/FS096 smoke scenarios и
  WeActTerm, как зафиксировано в project reports.
- Community-модуль CDC ACM для RV2 не распространяется. См.
  [заметку RV2](../../Misc/README.ru.md).

Скачивайте пять пакетов и checksum asset только для **одной** архитектуры из
[Release](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3). Имена
manifest: `SHA256SUMS-arm64.txt`, `SHA256SUMS-riscv64.txt` и
`SHA256SUMS-amd64.txt`. Проверьте набор командой `sha256sum -c <manifest>`.
Бинарные пакеты относятся к Release assets, не к Git.

## ARM64 assets — Debian 13

| Файл | SHA-256 |
|---|---|
| `libweact1_1.0.0-3_arm64.deb` | `a0202e035bca16bd37eeae2b9f4e3a3f6b5966114827d4ff05eeb2c24b098978` |
| `libweact1-dbgsym_1.0.0-3_arm64.deb` | `ce91eabf4fc29ce3e97932931b96650540446c05138c54a8209353ede3b00955` |
| `libweact-dev_1.0.0-3_arm64.deb` | `bab6599a4dbc604d2a0e2e0e01b3a2fba0fb1568e730cda97958f890ffad913f` |
| `weactterm_1.0.0-3_arm64.deb` | `c9237d6186897fa8644bb1e29e49b6c74c4892a6e09a2a54b0dd0bb0fc6c301a` |
| `weactterm-dbgsym_1.0.0-3_arm64.deb` | `97d7dc836d6cec97273ad136611e05972a8955dd965aa9812f77342f6e738aef` |

## RISC-V assets — Ubuntu 24.04.5 LTS на RV2

| Файл | SHA-256 |
|---|---|
| `libweact1_1.0.0-3_riscv64.deb` | `ca9a157c170c9dace0ecb882a7b2ef8246a0935aa41972e35f83e832a19e3d78` |
| `libweact1-dbgsym_1.0.0-3_riscv64.ddeb` | `068786ee043f2bc90b03137e1c2d0bc5882946ec5e7181f8af857aec4a1d9fe9` |
| `libweact-dev_1.0.0-3_riscv64.deb` | `ef5e5425d8a7ff8932107d9c411aa8d9af519c65b9e98f5d69094efc3865f801` |
| `weactterm_1.0.0-3_riscv64.deb` | `622986c016dc92791ac24945c2bb5de0cd8e8052fc521a68fe658adb58d9ec3a` |
| `weactterm-dbgsym_1.0.0-3_riscv64.ddeb` | `376839d8d04eaede7a47a60eb5d332e6ea536ff485d2c35c9601f19573499778` |

До установки проверьте release-provided manifest:

```sh
sha256sum -c SHA256SUMS
```

Для обычной установки используйте только три non-debug packages. Hardware
success остаётся visual/manual evidence; external barcode scanning,
побайтный LCD capture и расширенные reconnect matrices не заявляются.


## AMD64-пакеты — Debian 13 x86-64

Пять бинарных пакетов AMD64 собраны для Debian 13 и прошли локальные проверки
checksum, lintian, ABI и dependencies. Пользователь подтвердил установку
`libweact1`, `libweact-dev` и `weactterm`; затем опубликованные файлы были
скачаны из Release и прошли проверку SHA-256. ABI сохранён: SONAME
`libweact.so.1`, 79 экспортов, upstream API `1.0.0`.

| Файл | SHA-256 |
|---|---|
| `libweact-dev_1.0.0-3_amd64.deb` | `54a24568b224eee2bcde18166bd73fd3e1e8e2a1d2675cb8999c236d540b239f` |
| `libweact1-dbgsym_1.0.0-3_amd64.deb` | `546d6416e01031d6be2ab3b41d6cfb6ab9637ff39408c3634454f35f3b7a4754` |
| `libweact1_1.0.0-3_amd64.deb` | `11e49a42d8be3d8ffdcd0c0fc59d29d407b3d08da2702bc62a79ce6348571a24` |
| `weactterm-dbgsym_1.0.0-3_amd64.deb` | `99477e6304bebf722dac40e50a902151521b274461414377c8b4cb3ab2c48a3d` |
| `weactterm_1.0.0-3_amd64.deb` | `47fc018e4f4fdda21b571ce94fb03ec5fa89517f3288793f03be5d2bd7709386` |

На Debian 13 AMD64 пользователь визуально подтвердил очистку FS35, текст, QR
и вывод QR в области; true overlay FS096 сохранил QR под крупным текстом;
первый fullscreen Code128 после переподключения прошёл в трёх попытках с новым
USB-кабелем. В одной из двух дополнительных попыток со старым кабелем
наблюдалось смещение изображения; это не доказывает дефект ПО. Cold-start QR,
обычный текст FS096 и отдельные подробности выхода из терминала/освобождения USB
не подтверждены. Точные аппаратные команды и exit codes не предоставлены.

Два AMD64 debug-symbol файла сохраняют штатное расширение Debian `.deb`.
Шестой AMD64 asset — `SHA256SUMS-amd64.txt`. Скачайте его вместе с пятью
AMD64-пакетами и проверьте набор командой `sha256sum -c SHA256SUMS-amd64.txt`.
Файлы AMD64 debug symbols сохраняют штатное расширение Debian `.deb`.
