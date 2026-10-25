# Orange Pi RV2 CDC ACM note

The Orange Pi RV2 Ubuntu 24.04.5 LTS kernel `6.6.63-ky` used for WeAct35 has `CONFIG_USB_ACM` disabled. FS35 and FS096 can therefore appear in `lsusb` but have no `/dev/ttyACM*` node.

A community external module was evaluated from [Tolcen/ttyACM-driver-for-OrangepiRV2](https://github.com/Tolcen/ttyACM-driver-for-OrangepiRV2), commit `a78a0f46aaf9a82fb44cf135c808f3c5f7057625`. The manually built `cdc-acm.ko` for this exact kernel/riscv64 combination had SHA-256 `2f1501a5d038e1c082ee8f7f0281430c9e15fc201ad52b71375ba2c83e6e08ef` and was manually loaded and used on the recorded RV2.

## Distribution status

This repository deliberately does **not** redistribute that source archive or binary module. The community repository has no repository-level LICENSE/COPYING file and its changes cannot be authenticated against the exact vendor-kernel source base. SPDX markers in the source indicate GPL-family licensing, but that alone does not resolve provenance and redistribution-compliance risk. Obtain and review the upstream source yourself.

The module is not part of libweact, is not packaged by WeAct35, is not an automatic load target, and is not a universal Linux module. It applies only to the exact `6.6.63-ky` RV2/riscv64 environment recorded above. Other kernel builds are unsupported. Loading an external module may taint the kernel and can crash the system.

## Manual, owner-controlled use

After independently obtaining the matching source/module, verify both running kernel and digest before any load:

```sh
uname -r
sha256sum cdc-acm.ko
sudo insmod ./cdc-acm.ko
ls -l /dev/ttyACM*
```

Do not configure autoloading or add the module to a system module directory from these instructions. After the manual load, use `weact-list --verbose` to inspect devices before any write. Give the operating user access through the `dialout` group and re-login; do not use `chmod 777` on a tty device. If `/proc` ownership inspection is inconclusive for an otherwise verified display, `--allow-unknown-occupancy` is a narrow override only; it never accepts a known-busy or wrong device.
