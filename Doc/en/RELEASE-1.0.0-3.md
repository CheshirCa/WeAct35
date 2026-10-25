# WeAct35 1.0.0-3 release notes

This release asset set contains native Debian packages for two separately
validated platforms: Orange Pi 3 LTS / Debian 13 `arm64`, and Orange Pi RV2 /
Ubuntu 24.04.5 LTS `riscv64`. It does not replace the preserved ARM64
`1.0.0-2` asset set or rewrite the existing `v1.0.0` tag.

## Highlights

- FS096 fullscreen sessions request landscape before bitmap output, fixing the
  cold-start splash/orientation case. A true text overlay preserves device
  state.
- ABI is unchanged: SONAME `libweact.so.1`, 79 exports, pkg-config `1.0.0`.
- User-confirmed ARM64 evidence covers FS096 cold-start QR and Code 128,
  FS096 true overlay, FS35 text, WeActTerm, and Midnight Commander.
- User-confirmed RISC-V evidence covers FS35/FS096 smoke scenarios and
  WeActTerm as recorded in the project reports.
- The community RV2 CDC ACM module is not redistributed. See
  [the RV2 note](../../Misc/README.md).

Download all five artifacts and the matching architecture-specific checksum
asset for **one** architecture. The Release upload copies are named
`SHA256SUMS-arm64.txt` and `SHA256SUMS-riscv64.txt` so every asset filename is
unique. Verify the selected set with `sha256sum -c SHA256SUMS-arm64.txt` or
`sha256sum -c SHA256SUMS-riscv64.txt`. The source package directories keep
their original `SHA256SUMS` filenames. Release assets are not committed to Git.

## ARM64 assets — Debian 13

| File | SHA-256 |
|---|---|
| `libweact1_1.0.0-3_arm64.deb` | `a0202e035bca16bd37eeae2b9f4e3a3f6b5966114827d4ff05eeb2c24b098978` |
| `libweact1-dbgsym_1.0.0-3_arm64.deb` | `ce91eabf4fc29ce3e97932931b96650540446c05138c54a8209353ede3b00955` |
| `libweact-dev_1.0.0-3_arm64.deb` | `bab6599a4dbc604d2a0e2e0e01b3a2fba0fb1568e730cda97958f890ffad913f` |
| `weactterm_1.0.0-3_arm64.deb` | `c9237d6186897fa8644bb1e29e49b6c74c4892a6e09a2a54b0dd0bb0fc6c301a` |
| `weactterm-dbgsym_1.0.0-3_arm64.deb` | `97d7dc836d6cec97273ad136611e05972a8955dd965aa9812f77342f6e738aef` |

## RISC-V assets — Ubuntu 24.04.5 LTS on RV2

| File | SHA-256 |
|---|---|
| `libweact1_1.0.0-3_riscv64.deb` | `ca9a157c170c9dace0ecb882a7b2ef8246a0935aa41972e35f83e832a19e3d78` |
| `libweact1-dbgsym_1.0.0-3_riscv64.ddeb` | `068786ee043f2bc90b03137e1c2d0bc5882946ec5e7181f8af857aec4a1d9fe9` |
| `libweact-dev_1.0.0-3_riscv64.deb` | `ef5e5425d8a7ff8932107d9c411aa8d9af519c65b9e98f5d69094efc3865f801` |
| `weactterm_1.0.0-3_riscv64.deb` | `622986c016dc92791ac24945c2bb5de0cd8e8052fc521a68fe658adb58d9ec3a` |
| `weactterm-dbgsym_1.0.0-3_riscv64.ddeb` | `376839d8d04eaede7a47a60eb5d332e6ea536ff485d2c35c9601f19573499778` |

Before installing, verify the release-provided manifest with:

```sh
sha256sum -c SHA256SUMS
```

Use only the three non-debug packages for a normal installation. Hardware
success remains visual/manual evidence; external barcode scanning, byte-level
LCD capture, and broader reconnect matrices are not claimed.
