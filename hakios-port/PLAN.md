# HakiOS on the POCO X3 NFC (surya): port plan

Started 2026-11-01. Separate from ios-surya (the ios-sweet based port, which stays as it is) and from the
A5 2017 work. HakiOS runs from RAM: `fastboot boot` of a UEFI + HakiOS's EFI loader, system image sent into RAM,
nothing written to the phone except the loader's log (docs/concepts/architecture.md). Our iOS root in
`userdata` (ios-surya) is not touched by it.

## What is here
- `surya/`: the device folder made by `tools/hakios-device.py new surya --soc sm7150 --soc-title "Snapdragon 732G
  (SM7150)" --title "POCO X3 NFC" --vendor Xiaomi --maintainer android-a5y7lte --fdt surya.dtb` (dtb: Mu-Silicium's
  Resources/DTBs/surya.dtb, the downstream Android tree), with a new SoC layer `soc/` for sm7150 and every phone
  driver left out. To become its own repository (hakios-surya) later.
- `surya/soc.args`: 11 values read from the tree; added by hand: `soc_rsc=0x18220000` (apps_rsc mailbox@18220000),
  `soc_mpss_mem=0x8b000000` (modem_region). Still open: `soc_cmd_db` (SM7150 has a cmd-db pointer at 0xc3f000c,
  no reserved node), `soc_ipcc` (SM7150 has no IPCC; APCS mailbox instead: only matters for the modem).

## Steps (HakiOS docs/porting/index.md order)
1. PC set-up: clone hakios-xnu-kernel, `common/setup.sh`; Linux toolchain (theos + iPhoneOS16.5.sdk,
   `xnu-deps/setup.sh`); `make qemu-open` as the first check that the toolchain works (no Apple or phone files).
2. Put `surya/` into `devices/surya`, `make surya_defconfig && make config`; `make kernel`, `make loader`.
3. UEFI for surya: Mu-Silicium has `Platforms/Xiaomi/suryaPkg` (Linux build with edk2 tooling); or HakiOS's own
   U-Boot. Combine with the loader as hakios-uefi does for redwood.
4. First boot with nothing of the phone's (`make phone SECS=120`): loader banner, kernel lines, launchd; the
   display shows UEFI's framebuffer.
5. Drivers one at a time: cpufreq/thermal/socinfo (SM7150 values) -> buttons + touch (Novatek NT36xxx on the
   Huaxing panel: a new driver, not Goodix) -> UFS -> GPU (Adreno 618) -> audio/battery -> Wi-Fi (WCN3991/
   WCN3950 family, not WCN6750) -> rest.
Firmware: GPU zap `a615_zap.mbn` in sm7150-mainline/firmware-xiaomi-surya; `a630_sqe.fw`, `a618_gmu.bin` in
linux-firmware. The phone runs no Android now (userdata holds the iOS root), so `pull-firmware.sh` over adb is not
possible as is; take firmware from those repos or a stock vendor image.
