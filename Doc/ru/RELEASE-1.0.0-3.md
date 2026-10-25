# WeAct35 1.0.0-3 — release notes

Этот набор release assets содержит native Debian packages для двух отдельно
проверенных платформ: Orange Pi 3 LTS / Debian 13 `arm64` и Orange Pi RV2 /
Ubuntu 24.04.5 LTS `riscv64`. Он не заменяет сохранённый набор ARM64
`1.0.0-2` и не переписывает существующий тег `v1.0.0`.

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

Получайте все пять artifacts и соответствующий checksum asset только для
**одной** архитектуры. Копии для загрузки в Release называются
`SHA256SUMS-arm64.txt` и `SHA256SUMS-riscv64.txt`, чтобы имена всех assets
были уникальны. Проверяйте выбранный набор командой
`sha256sum -c SHA256SUMS-arm64.txt` или `sha256sum -c SHA256SUMS-riscv64.txt`.
В исходных каталогах пакетов manifests остаются под именем `SHA256SUMS`.
Release assets не коммитятся в Git.

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
