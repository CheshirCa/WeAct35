# WeAct35 1.0.0-3 release notes

GitHub Release [v1.0.0-3](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3)
contains native Debian packages for Debian 13 `amd64`, Debian 13 `arm64`, and
Orange Pi RV2 / Ubuntu 24.04.5 LTS `riscv64`: five packages and one checksum
manifest for each architecture, 18 assets total. AMD64 assets were downloaded
from the Release and passed SHA-256 verification. Existing package versions and
the historical tag are unchanged.

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

Download the five artifacts and matching architecture-specific checksum asset
for **one** architecture from the [Release](https://github.com/CheshirCa/WeAct35/releases/tag/v1.0.0-3).
The manifests are `SHA256SUMS-arm64.txt`, `SHA256SUMS-riscv64.txt`, and
`SHA256SUMS-amd64.txt`. Verify the chosen set with `sha256sum -c <manifest>`.
Binary packages are Release assets, not Git files.

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

## AMD64 packages — Debian 13 x86-64

The five AMD64 binary packages were built for Debian 13 and passed local
checksum, lintian, ABI, and dependency checks. The user confirmed installation
of `libweact1`, `libweact-dev`, and `weactterm`; the published files were then
downloaded from the Release and passed SHA-256 verification. ABI remains SONAME
`libweact.so.1`, 79 exports, upstream API `1.0.0`.

| File | SHA-256 |
|---|---|
| `libweact-dev_1.0.0-3_amd64.deb` | `54a24568b224eee2bcde18166bd73fd3e1e8e2a1d2675cb8999c236d540b239f` |
| `libweact1-dbgsym_1.0.0-3_amd64.deb` | `546d6416e01031d6be2ab3b41d6cfb6ab9637ff39408c3634454f35f3b7a4754` |
| `libweact1_1.0.0-3_amd64.deb` | `11e49a42d8be3d8ffdcd0c0fc59d29d407b3d08da2702bc62a79ce6348571a24` |
| `weactterm-dbgsym_1.0.0-3_amd64.deb` | `99477e6304bebf722dac40e50a902151521b274461414377c8b4cb3ab2c48a3d` |
| `weactterm_1.0.0-3_amd64.deb` | `47fc018e4f4fdda21b571ce94fb03ec5fa89517f3288793f03be5d2bd7709386` |

On Debian 13 AMD64, user-confirmed visual results cover FS35 clearing, text,
QR, and QR region output; FS096 true overlay retained QR under large text; and
first fullscreen Code128 after reconnect passed three attempts using a new USB
cable. One of two additional attempts using an older cable showed image shift;
this observation does not establish a software defect. Fullscreen QR cold-start,
ordinary FS096 text, and some terminal-exit/USB-release details were not
confirmed. Exact hardware commands and exit codes were not supplied.

The two AMD64 debug-symbol artifacts keep Debian's generated `.deb` extension.
The sixth AMD64 asset is `SHA256SUMS-amd64.txt`. Download it alongside all five
AMD64 packages, then verify with `sha256sum -c SHA256SUMS-amd64.txt`. The AMD64
debug-symbol files retain Debian's generated `.deb` extension.
