# ios-surya — port notes (POCO X3 NFC, `surya`)

Fork of [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet). The
upstream `NOTES.md` is the sweet (Redmi Note 10 Pro) history and stays as
reference. **This file is the handoff for the surya port.**

## Target

| | |
| --- | --- |
| Phone | Xiaomi POCO X3 NFC, codename `surya` |
| SoC | Qualcomm SM7150 (Snapdragon 732G), same as sweet |
| Panel | **Huaxing** variant (confirmed by the owner while testing postmarketOS) |
| Dev PC | Linux Mint |

## Status

- **2026-10-23 ~18:50 — real battery level works** with `ARGS="sweetbattery=1"`
  (upstream's read-only PM6150 polling, SID0 0x10-0x13/0x48): status bar showed 49%;
  log `SWEET BATTERY: PM6150 / estimated SOC, 48% 3844mV external=1 charging=1`.
  Voltage-based estimate (no fuel-gauge calibration); upstream's 5020 mAh nominal is
  sweet's, surya is 5160 mAh (cosmetic). Test 6 log also shows PosterBoard still
  crashes on every boot (see Wallpaper section). Owner OK with replacing the dead
  Ubuntu Touch recovery by LineageOS recovery: flashed to `recovery` 2026-10-23 ~19:00, Power+VolUp now boots it (log pulls no longer need `fastboot boot`).

- **2026-10-23 ~18:22 — cpus=8 + sweetfb_hz=60 boots fine** (`CPUS=8 FBHZ=60
  tools/surya-test4.sh KERNEL 5`, A76s at MPIDR 0x600/0x700 via upstream patch 0078);
  owner: scrolling "a bit better". Still CPU-rendered. Open UI issues reported by the
  owner: brightness slider has no effect (no `/backlight` service; PM6150L WLED driver
  needed), no auto-brightness (no ALS), Today View / App Library show a plain white
  background (no wallpaper: PosterBoard crashes with sig 5 at start — under
  investigation), page swipes not fluid. Dark/light mode switch works.

- **2026-10-23 ~18:08 — 🎉🎉 MILESTONE: iOS HOME SCREEN ON SURYA, UNLOCKED BY TOUCH.**
  CI `xnu #2` (commit `fb711be`, patch 0082) kernel, `tools/surya-test4.sh KERNEL 5`
  (cpus=6, `sweettouch=2`, firmware as `TouchFW` blob), `fastboot boot`. The owner swiped
  up on the lock screen and reached the home screen (photo
  `evidence/2026-10-23-HOMESCREEN-touch-unlock.jpg`: 4:02, charging icon, two "Waiting…"
  widgets, Photos / App Store / Settings, search pill, dock Phone / Safari / Messages
  with a badge). Swiping down from the top opens the notification view (iOS 16 shows it
  as the lock screen) — normal iOS behaviour. Novatek touch works end-to-end in XNU.

- **2026-10-23 17:36 — 🎉 TOUCH WORKS on surya (shim step 3c).** After the dummy-byte
  fix: `NVT BOOT ok=1`, `FWINFO valid=1 max_xy=0438 0960`, then the 60 s loop drew
  the owner's strokes across the **whole panel with 5 fingers at once** (finger 1 white,
  2 green, 3+ yellow), coordinates matching the drawing position (1080×2400, no
  rotation/flip needed). Sample `TOUCH XY x=0x22? y=0x74A`, `x=0x251 y=0x70D`.
  Photo: `evidence/2026-10-23-touch-3c-WORKING-multitouch.jpg`. Only remaining touch
  work: port into XNU (step 3d).

- **2026-10-23 ~17:07 — cpus=6 works.** `CPUS=6 tools/surya-test4.sh KERNEL 5` →
  `shim-boot-surya-test5-cpu6.img`, `fastboot boot`: owner reports it reached the lock
  screen much faster ("like a real iPhone") than the ~15 min cpus=1 boot. Default for
  further surya boots: cpus=6. (Exact time not measured.)
- **Touch firmware located:** `sm7150-mainline/firmware-xiaomi-surya` (commit `971dfbf9`)
  `usr/lib/firmware/qcom/sm7150/xiaomi/surya/novatek_ts_huaxing_fw.bin`, 139264 bytes,
  SHA256 `4e519856…6aab` (Tianma variant alongside). Kept out of git (vendor blob).

- **2026-10-23 16:57 — persistent log pulled (Test 5 boot).** `fastboot boot` of the
  official LineageOS Recovery for surya (`recovery.img`, nothing flashed), Advanced →
  Enable ADB gives `uid=0(root)`; `tools/sweet-log-pull.sh logs/test5.txt` read
  `/dev/block/sda16` (userdata, 112556126208 bytes): `# version 1, ring 4194304,
  total 315852 bytes, flush #154`, 4738 lines (log kept out of git: `logs/`). Findings:
  - `SWEET UFS: "userdata" on LUN 0: first block 0x284a18, 107341 MiB, read-write`.
  - `SWEET KEYS: arb=0x50000000 apids=512 pon-apid=200 gpio4-apid=241 ...` — SPMI arbiter
    and both APIDs found exactly as on sweet. Events: key=0 power (HID 0x30), key=1
    vol-up (PM6150L GPIO2, HID 0xe9), key=2 vol-down (PON resin, HID 0xea) — all three
    deliver HID events to backboardd.
  - `SWEET TOUCH: disabled (sweettouch=0)`; `SweetHIDTouch open by IOHIDEventServiceUserClient`.
  - `SWEET FB: backboardd sel 12` (RequestPowerChange) on the power press; 6000 swaps logged.
  - Only 2 crashes: `driverkitd` sig 9 (no DriverKit; harmless) and `PosterBoard` sig 5
    (lock-screen wallpaper renderer; likely why the wallpaper is a plain gradient).
  - Log ends with Power+Vol-down held → `exec sosd` (iOS Emergency SOS / power-off UI)
    just before the forced reset into fastboot — expected.

- **2026-10-23 ~16:35 — 🎉 MILESTONE: iOS 16.7.16 LOCK SCREEN ON THE POCO X3 NFC (surya).**
  Same Test 5 boot (cpus=1), roughly 15 min from `fastboot boot`. Photos:
  `evidence/2026-10-23-test5-LOCKSCREEN-1.jpg`, `-2.jpg` (clock advances 4:16 → 4:17, so
  SpringBoard/backboardd keep rendering), `-boot-10-console.jpg` (Skywalk/identityservices
  console lines just before). Observed: "Wednesday, December 31" (no RTC → epoch date),
  charging battery icon (static IOPMPowerSource), **iPhone X gesture layout** (home
  indicator bar, camera button, no Home button → /product identity from d22ap.adt works),
  CPU rendering on the Huaxing panel. Touch not expected (sweettouch=0).
- **Power button works on surya (patch 0060 key driver, unchanged).** Press 1: lock UI
  removed, wallpaper only (`evidence/2026-10-23-test5-power-press-off.jpg`) — iOS display
  off; the 0079 fade-to-black does not switch the LCD backlight (PM6150L WLED), so
  "off" stays lit — cosmetic TODO. Press 2: lock screen back at 4:21 with charging icon
  and a new blue status icon (`-power-press-wake.jpg`).
- **Volume up/down work** (owner: iOS volume HUD appears on each press) → PM6150L GPIO2
  vol-up and PON resin vol-down match sweet. Home indicator bar does the normal
  "swipe up" hint animation. Blue status icon under the battery still unidentified
  (check the log). Photo: `evidence/2026-10-23-test5-lockscreen-4-23.jpg`.

- **2026-10-23 16:20** — **Test 5: iOS userland boots on surya — Apple logo.**
  `fastboot -S 512M flash userdata ios16-root.simg` (24/24 OKAY, 377 s; owner approved
  the erase — Ubuntu Touch gone). ABL hung on the next `fastboot boot` ("Sending"),
  fixed by unplug + Power+VolDown ~15 s (same Xiaomi ABL quirk as sweet). Then
  `fastboot boot shim/out/shim-boot-surya-test5.img` (SHA256 `450da8e9…38a5`).
  Photos `evidence/2026-10-23-test5-boot-01..09.jpg`. Observed:
  - Kernel as in Test 4, then HFS root from userdata, launchd and system daemons
    (`exec` xpcproxy, budd, clipserviced, contactsd, bluetoothd, fairplayd.H2, geod,
    powerd, lsd, nsurlsessiond, …), SWEET AKS selector traffic, Sandbox stand-in.
  - backboardd: `open AppleCLCD`, `SWEET FB: backboardd sel 57/15/22/29/33/79 -> 0x0`,
    `IODeviceTree:/product -> IOPlatformDevice` (iPhone X identity present),
    `MobileGestaltHelper sel 0/28 -> 0x0`, `open SweetHIDTouch` / `SweetHIDButtons`.
  - IOAcceleratorES matching -> 0 found (no GPU, expected: software rendering).
  - `CRASH driverkitd reason ns 1 code 0x7` (not fatal; boot continued).
  - Screen: **Apple logo** rendered by iOS on the Huaxing panel.

- **2026-10-23 15:48** — **Test 4b PASSED: kernel UFS driver reads userdata on surya.**
  `tools/surya-test4.sh KERNEL 4b` (adds `rd=ufsroot0`, still no `ufsrw`), image SHA256
  `8e68658a…938d`, `fastboot boot`. Photos: `evidence/2026-10-23-test4b-*.jpg`. Observed:
  - `BSD root: ufsroot0, major 1, minor 0` → kernel UFS driver attached and exposed userdata.
  - `hfs_ValidateHFSPlusVolumeHeader: unknown Volume Signature : e0`,
    `hfs_mountfs returned error=22`, `cannot mount root, errno = 19`, `mount(N) failed`,
    retried in a loop (N up to 13+) → HFS read the ext4 bytes at the volume-header offset
    and rejected them. Expected: userdata is still Ubuntu Touch ext4.
  - Bonus: `SWEET USB: DWC3 GEVNTADR ... GHWPARAMS0 0x4020400a`, `SWEET USB: QUSB2
    PLL_STATUS 0x17171717 ...` → USB controller registers respond (relevant to the
    optional USB live log, patches 0072–0077).
  - No writes possible (driver EROFS without `ufsrw`); phone returned to Ubuntu Touch.

- **2026-10-23 15:43** — **Test 4 PASSED: XNU boots on surya.** Image
  `shim/out/shim-boot-surya-test4.img` (SHA256 `a9f09650…f682`) built on the owner's
  PC with `tools/surya-test4.sh` from CI kernel `xnu #1` (unchanged upstream patches),
  `fastboot boot`, nothing flashed, UFS writes disabled. Photo:
  `evidence/2026-10-23-test4-xnu-kernel-boot.jpg`. Observed:
  - `SWEET CPU: requested=1 selected=1 (limit=8)`, `cpu=0 mpidr=0x0 came alive; SGI1 OK`,
    `1/1 CPUs initialized, SGI1 verified` → GICv3, timer, scheduler OK.
  - `SWEET FB: AppleCLCD 1080x2400 stride 4320 at 0x9c000000 ... presented as 1125x2436`.
  - `SWEET WDT`, `SWEET SURF: IOCoreSurfaceRoot registered`, `SWEET HID: buttons registered`,
    `SWEET HID: touch registered` (service only; `sweettouch=0`), `SWEET BATTERY: static fallback`.
  - BSD init: MAC framework, SWEET AMFI + permissive Sandbox, `SWEET HFS: hfs-627.100.6
    registered`, mbinit, Skywalk self-tests PASSED, lo0/gif0/stf0 attached.
  - Final: `Waiting on <dict>...IOProviderClass IOMedia ... Content Apple_HFS</dict>` —
    the expected stop: no root device given (no `rd=ufsroot0`), no HFS root exists yet.

- **2026-10-23 15:12** — **Test 3 PASSED.** `fastboot boot shim/out/shim-boot-surya.img`
  (SHA256 `7c9048e2…28c9`, nothing flashed). Photo:
  `evidence/2026-10-23-test3-shim-ufs-probe.jpg`. Observed on screen:
  - Red console with readable white/yellow text, correctly aligned → framebuffer
    `0x9c000000`, 1080×2400, stride 4320, a8r8g8b8 is **correct on surya** (same as sweet).
  - `INITRD lo=856FE000 hi=856FE045`, `LOADER no SWEETXNU payload` (expected: empty ramdisk).
  - Own UFS driver works read-only: `ver=00000210` (UFS 2.1), `cap=1587031F`,
    TUR/CAP/READ rc=0, LUN0 `lastlba=01CB97FF blk=00001000` (~123 GB, 128 GB model).
  - GPT read: minidump, cust (ext4), recovery, cache (ext4), exaid, super (8 GiB),
    **userdata lba=0x00284A18, 0x1A34D MiB (~105 GiB), ext4**.
  - Ends with `UFS probe done, reset in ~3 min` (shim then resets via PSCI).

- **2026-10-23** — Test 1 done: live DT + iomem read from the phone under
  Ubuntu Touch (read-only). Board layer matches sweet except touch (see table).

- **2026-10-22** — Feasibility check done (read-only, nothing built, nothing
  run on the phone). Verdict: realistic. The SoC core is shared with sweet; the
  board layer must change, and the touch driver is the main new work.

## Rules for this port

- The upstream `AGENTS.md` approval to erase `userdata` was given by the sweet
  owner for sweet. **It does not apply here.** Every destructive step on surya
  (flashing `userdata`, anything beyond `fastboot boot`) needs explicit
  approval from the surya owner first, with the risk explained.
- Never boot or flash a sweet image on surya.
- Back up `persist`, `modemst1`, `modemst2`, `fsg`, `fsc`, `boot`, `dtbo`,
  `vbmeta` before any install step. Keep dumps out of git (`backup/`, `dtb/`
  are already ignored).
- Go one step at a time; record observed results here and keep them separate
  from guesses.

## Live hardware map (2026-10-23, from this phone)

Source: `/sys/firmware/fdt` and `/proc/iomem` read under Ubuntu Touch (Halium
kernel, ABL-provided DT). The dump itself stays out of git (it contains the
serial number); only values are recorded here.

| Item | Surya value (observed) | Same as sweet? |
| --- | --- | --- |
| Board | `qcom,msm-id = <0x16d 0>`, `qcom,board-id = <0x22 0>`, `hwname=surya` | — |
| RAM (`/memory`) | `0x80000000` + 2 GiB, `0x1_00000000` + `0xfcc00000` | Yes |
| Upper bank | iomem: System RAM `a1600000-1fcbfffff`, no carve-outs above 4 GiB | Yes (`--highmem` range fits) |
| Low carve-outs | `0x85700000` … `0x9f800000+0x1e00000` (all below `0xb0000000`) | Same layout pattern |
| Splash framebuffer | `cont_splash_region@9c000000`, size `0x1800000` | Same address (sweet `FB_SIZE` 0x1700000 fits) |
| Active panel | `msm_drm.dsi_display0=dsi_nt36672c_huaxing_fhd_video_display` | Different panel |
| Panel mode | `dsi_video_mode`, DSC v1.1, 1080×2400 (`0x438`×`0x960`), 120 Hz, panel bpp 24 | Video + DSC like sweet; size same |
| GIC | GICD `0x17a00000`/0x10000, GICR `0x17a60000`/0x100000, stride 0x20000 | Yes |
| Timer | `arm,armv8-timer`, PPIs 1/2/3/0, 19.2 MHz (`0x124f800`) → vtimer INTID 19 | Yes |
| CPUs | 8 cores, MPIDR `0x000`…`0x700` | Yes |
| PS_HOLD | `restart@c264000` | Yes (`shim/shim.c` PSHOLD) |
| Restart reason | `qcom,msm-imem@146aa000` + `restart_reason@65c` → `0x146aa65c` | Yes |
| UFS | `ufshc@1d84000`, `androidboot.bootdevice=1d84000.ufshc` | Yes |
| Power key | PM6150 PON (`kpdpwr`/`resin`) | Yes |
| Volume up | `gpio-keys` → PM6150L (SID 4) GPIO 2, code 115 | Yes (sweet patch 0060 reads the same block) |
| Touch | `spi@0x880000` (`qcom,spi-geni`, QUP0 SE0, IRQ SPI 0x259) → `novatek,NVT-ts-spi` @CS0, 8 MHz, reset GPIO 8, irq GPIO 9; supplies vddio + LAB/IBB (in-cell, shared with panel) | **No** (sweet: Goodix I²C) |
| Debug UART | `earlycon=msm_geni_serial,0x4a90000` in cmdline | Unknown if reachable (test pads) |

Takeaway: apart from touch, every address the shim, `mkadt.py` and patch 0060
hard-code matches surya. Still unknown: splash pixel format/stride (sweet:
a8r8g8b8, stride 4320) and the stock `boot.img` header — both checked next.

## Verified: sweet vs surya (mainline DT)

Sources: ios-sweet tree at fork time, and
[sm7150-mainline/linux](https://github.com/sm7150-mainline/linux) commit
`fd78d179` (`arch/arm64/boot/dts/qcom/`).

| Subsystem | sweet (what ios-sweet uses) | surya | Port impact |
| --- | --- | --- | --- |
| GIC, timer, PSCI | GICD `0x17a00000`, GICR `0x17a60000`, vtimer INTID 19, PSCI via SMC (`shim/shim.c`, `tools/mkadt.py`) | Same SoC (`sm7150.dtsi`) | Expected reusable; confirm from live DT |
| UFS host | `ufshc@1d84000` (`tools/mkadt.py`) | Same node in `sm7150.dtsi` | Expected reusable |
| Panel | Samsung `ams667yr01` AMOLED, DSC video mode, 1080×2400 (`sm7150-xiaomi-sweet.dts`, `NOTES.md`) | Huaxing LCD with Novatek in-cell touch, 1080×2400 | Framebuffer address and panel mode must be checked on the phone |
| Framebuffer | ABL splash at `0x9c000000`, a8r8g8b8, stride 4320 (`shim/console.h`) | Unknown | Read `cont_splash`/`splash` region from live DT |
| Touch | Goodix GT9896, **I²C** `i2c7`, SE at `0xa84000`, addr `0x5d`, avdd L11C (`0060`, `0062`, `0063` patches; sweet dts) | Novatek `nt36xxx-spi` on **SPI** `spi0` (`0x880000`), vio L18A (`sm7150-xiaomi-surya.dtsi`) | **New driver**: GENI SPI + Novatek protocol + power |
| Touch GPIOs | reset GPIO 8, irq GPIO 9 | reset GPIO 8, irq GPIO 9 | Same pins |
| Buttons | PMIC PON + volume via SPMI (`0060` patch) | PM6150/PM6150L; `KYPD_VOLP_N` on pm6150l gpio2 | Probably small; verify volume wiring |
| RAM | upper bank `0x100000000` + `0xfcc00000` (`tools/mkadt.py --highmem`) | Unknown for this unit | Read memory + reserved-memory from live DT |

## Build (surya)

- `shim/build-surya.sh` builds the shim with clang/lld (`--target=aarch64-linux-gnu`,
  plus `-fno-jump-tables`, `-fno-builtin-strcmp` and `shim/compat.c` so the
  no-absolute-relocation rule still holds) and packs it with
  `tools/mkbootimg_v2.py` (pure-Python boot.img v2). Output:
  `shim/out/shim-boot-surya.img`. Tested: boot.img round-trips through
  `tools/unpack_bootv2.py` with the kernel byte-identical.
- Stock surya boot header (owner's `boot` partition, 2026-10-23): v2, page 4096,
  kernel 0x8000, ramdisk 0x1000000, tags 0x100, dtb 0x1f00000, dtb size 0x612f7
  at file offset 0x145f000 — identical offsets to sweet's `shim/build.sh`.
- Needs `dtb/surya/surya-boot.dtb` (the DTB section of that boot.img; gitignored).
- Test 2 done 2026-10-23: owner holds read-only backups of persist, modemst1/2,
  fsg, fsc, boot, dtbo, vbmeta, vbmeta_system with SHA256SUMS on his PC.

## Test 4 prep (2026-10-23)

- CI run `xnu #1` on branch `surya` (commit `3a227cc`): **success**, 12m36s,
  artifacts `xnu-kernel` + `xnu-buildtree`. Kernel unchanged from upstream
  (board addresses identical, see table).
- `tools/surya-test4.sh KERNEL` runs on the owner's PC: `mkadt.py --highmem`
  (no /product needed for a kernel-only boot), `mkpayload.py` with
  `cpus=1 sweettouch=0 sweetkeys=0 sweetlog=0` and **no `ufsrw`** (kernel UFS
  driver stays read-only), then `shim/build-surya.sh`.
- Pass criterion: kernel log scrolls on screen; expected stop at waiting for root.

## Test 5 prep (2026-10-23, owner's PC)

- IPSW `iPhone10,3,iPhone10,6_16.7.16_20H392_Restore.ipsw` (SHA1 `5606c298…0193`,
  6,171,035,648 bytes) from updates.cdn-apple.com; OS `087-86698-044.dmg`,
  SystemOS `087-86593-047.dmg`, AppOS `087-86715-047.dmg`.
- apfs-fuse built from source (not in Mint repos).
- `mkiosroot.sh` → `~/ios-ipsw/ios16-root.img` (16G, fsck OK) →
  `sweet-root-tweaks.py` (7 _PanicOnCrash renamed, 2 CommCenter jobs removed;
  warning: no mobileactivationd system container, data ark skipped) →
  `ios16-root.simg` (12G) SHA256 `e2555a15…dbf8`.
- `tools/surya-test4.sh KERNEL 5` = README boot-args with `cpus=1 sweettouch=0`
  (buttons on, log ring on). Writes userdata — boot only after the flash.

## Touch plan (research 2026-10-23)

Hardware (live DT): `spi@0x880000` `qcom,spi-geni` = QUP wrapper 0 SE0 (wrapper
`qupv3_0_geni_se@0x8c0000`), clocks `se-clk`/`m-ahb`/`s-ahb` (GCC phandle 0x27, ids
0x44/0x67/0x68), IRQ SPI 0x259, GPI DMA available but not required; pins
`qupv3_se0_spi_active`: gpio49–52, function `qup00`; device `novatek,NVT-ts-spi` CS0,
8 MHz, reset GPIO 8, irq GPIO 9 (falling), `novatek,swrst-n8-addr = 0x3f0fe`,
`novatek,spi-rd-fast-addr = 0x3f310`; supplies vddio + LAB/IBB (panel rails, already on).

Reference driver: `sm7150-mainline/linux` (HEAD `fd78d179`)
`drivers/input/touchscreen/nt36xxx_{core,spi}.c`, `nt36xxx.h`:
- SPI framing (`nt36xxx_spi.c`): page select `ff XX YY` (addr bits 23..16, 15..7) as one
  write, then read = `addr&0x7f` byte followed by data; write = `(addr&0x7f)|0x80` + data.
  SPI mode 0, 8-bit, ≤ 8 MHz.
- Chip ID (`trim_id_table`): NT36675-class map for ids `xx xx xx 75 66 03` and
  `0C xx xx 72 66 03`; memory map `nt36675_memory_maps` (event buffer `0x22D00`,
  boot-ready `0x3F10D`, `SWRST_N8 0x3F0FE`, `SPI_RD_FAST 0x3F310`, eng reset `0x7FFF80`).
- **Firmware is host-loaded at every power-on**: `request_firmware(firmware-name)` →
  `_nt36xxx_boot_download_firmware` (ILM/DLM partitions parsed by `nvt_bin_header_parser`,
  HW CRC check). Blob: `novatek_ts_huaxing_fw.bin` (see above).
- Reports: `nt36xxx_report` reads `POINT_DATA_LEN` (65) bytes from the event buffer,
  10 fingers.

XNU side (sweet patch 0060 `bsd/dev/sweet_touch.c`): one polling kernel thread, GENI
register access through `ml_io_map`, GCC clock vote + TLMM GPIO setup, delivers
`sweet_hid_touch(finger, down, x, y)` to the existing SweetHIDTouch digitizer service
(0062/0063 add RPMh rail + Goodix packing). A surya driver can reuse the thread, GENI
helpers and `sweet_hid_touch`, replacing the bus (I²C → SPI on SE0) and protocol
(Goodix → Novatek incl. firmware download).

Steps:
- 3a read-only shim probe: GCC SE0/wrapper clock status, SE0 `GENI_FW_REVISION`
  (protocol: 1 = SPI expected), TLMM gpio8/9/49–52 config — no writes.
  Implemented: `shim/surya_touch.c` (`-DSURYA_TOUCH_PROBE`, only in build-surya.sh),
  runs on the no-payload path before the UFS probe. GCC: vote `0x5200c` bits 6–10,
  CBCRs `0x17004/08/0c/14/30` (from `drivers/clk/qcom/gcc-sm7150.c`); skips SE reads if
  the QUP0 AHB clocks report CLK_OFF.
  **Result 2026-10-23 17:24** (`evidence/2026-10-23-touch-3a-spi-probe.jpg`):
  GCC vote `0x5200c=0x41000000` (APPS votes none of QUP0 bits 6–10); CBCRs m_ahb=0,
  s_ahb=0, core=0x4220, core2x=0x4220 (all running, voted by someone else), **SE0 CBCR
  0x80004220 = CLK_OFF** (SE clock gated); QUP0 hw_ver 0x20000000 (v2.0 → SPI
  oversampling 1); **SE0 FW revision 0x101 → protocol 1 = SPI, firmware loaded**;
  status 0, if_dis 0, m_clk_cfg 0x11, clk_sel 0, cpha/cpol 0, word_len 0 (unconfigured),
  demux 3, DMA off, FIFOs empty, HW_PARAM 0x20102964 / 0x20104800. Userdata fs "other"
  (HFS iOS root intact). ⇒ Need: vote SE0 clock + set RCG, configure SPI regs; no
  firmware load for the SE.
- 3b SPI chip-ID read from the shim (first IC transaction; reads only).
  Implemented (`SURYA_TOUCH_STEP=2 shim/build-surya.sh`): GCC vote |= bits 6–10, SE0 RCG
  `0x17034` → XO/1 19.2 MHz (or stop if DFS level 0 isn't XO), TLMM gpio49–52 (west tile
  `0x3500000`) func 2 = qup00, 6 mA; SE0 FIFO mode, mode 0, CS0, 8-bit, clk div 4
  (4.8 MHz), packing 0x7f8fe/0xffefe (geni_se_config_packing 8/4/MSB); full-duplex
  M_CMD 3. Reads: chip ID at `0x1F64E` (6 bytes after cmd+dummy), fw state at
  `0x22D00|0x60`. No IC writes besides page-select frames.
  **Result 2026-10-23 17:28** (`evidence/2026-10-23-touch-3b-chip-id.jpg`): SE0 RCG
  cfg 0x00100000, **DFSR 0x21 (DFS on), level 0 = XO/1** → used as is; SE0 CBCR
  0x4220 (on), vote 0x410007c0; gpio49/52 0x1 → 0x88; gpio8 (reset) cfg 0x1 input
  pull-down io 0 (= reset deasserted per mainline GPIOD_OUT_LOW); gpio9 io 0;
  SE0 m_clk_cfg 0x41, word 4. **NVT ID rc 0, bytes `0C 00 00 72 66 03`** = mainline
  trim entry `{0x0C,FF,FF,0x72,0x66,0x03}` → NT36675 memory map, hw_crc 2. **SPI to the
  touch IC works.** NVT STATE bytes `1A 4C 28 32…`, fw_state 0x4C (not 0xA0–0xAF) →
  no firmware running, as expected.
- 3c firmware download from the shim (blob in the ramdisk) → event-buffer polling,
  print touches on screen. Implemented (`SURYA_TOUCH_STEP=3`, firmware as ramdisk):
  streaming FIFO SPI (TX-only / full duplex, 16-word FIFO), eng reset 0x5a@0x7FFF80,
  bootloader reset 0x69@0x3F0FE + 0@0x3F310, header parse (blob: ILM bin 0x100 → SRAM 0
  size 0x12ec7, DLM bin 0x18100 → 0x27140 size 0x41ef, 13 info sections, no overlay,
  FW_VER 0xc3/0x3c), partition writes in 4 KiB chunks, ILM/DLM descriptors, CRC enable,
  0xae host cmd, boot ready, wait fw_state A0–AF, then 60 s of event-buffer polling
  drawing dots at touch positions.
  **Try 1, 2026-10-23 17:33** (`evidence/2026-10-23-touch-3c-fw-try1.jpg`): ramdisk
  0x22000 bytes; ID2 after eng/bootloader reset still `0C 00 00 72`; 15 parts, ILM
  0x12ec7, DLM 0x41ef. `NVT BOOT ok=0` (checked the wrong byte) but **`NVT FWINFO`
  `C3 3C 12 24 04 38 09 60` = fw_ver 0xC3 / ~0x3C, 18×36 sensor, max 1080×2400 —
  the downloaded firmware IS running.** Bug: Linux regmap buffers keep the SPI dummy
  byte at [0], so Linux `buf[1]` = our `[0]`; fixed in the reset-state check and the
  point parser (`pt + 6*i`, 65 bytes). Touch loop now runs if FWINFO is valid.
- 3d port into XNU as `sweet_touch` Novatek path; firmware shipped in the root image
  or the payload. **Implemented 2026-10-23:** `xnu/patches/0082-surya-novatek-spi-touch.patch`
  (adds the Novatek SPI path to `bsd/dev/sweet_touch.c`; boot-arg `sweettouch=2`
  selects it, sweet's Goodix/Focaltech path untouched; thread starts after
  `sweettouchdelay` s, default 30; maps GCC/TLMM-west/SE0, clocks + pins + SPI setup
  as in the shim, checks chip id `0C xx xx 72 66 03`, eng reset + bootloader reset +
  firmware download (3 attempts), then polls the event buffer every 10 ms and feeds
  `touch_report()` → `sweet_hid_touch()`; ESD pattern → firmware reload). Firmware
  delivery: `tools/mkpayload.py --blob TouchFW=FILE` appends a segment and adds
  `/chosen/memory-map TouchFW = <phys, size>`; the kernel reads it via
  `ml_static_ptovirt`. `tools/surya-test4.sh KERNEL 5` adds the blob and `sweettouch=2`
  automatically when `research/fw/novatek_ts_huaxing_fw.bin` exists (SURYA_TOUCH=0 to
  disable). Syntax-checked locally with clang against stub headers; needs a CI build.

## Wallpaper / PosterBoard (research 2026-10-23)

`tools/sweet-crash.py` on the owner's PC (cache extracted from `ios16-root.img`,
`grep -a` needed because the log has binary bytes):
`CRASH PosterBoard sig 5` at **`-[PBFPosterExtensionDataStore
_stateLock_setupSwitcherConfigurationForFirstTime] + 2040`** (a trap/assert), called
from `_stateLock_updateExtensions:refreshDescriptors:…` ← `executeUpdate:…` ←
`prewarmWithCompletion:` ← `PBFPosterExtensionDataStoreXPCServiceGlue
server:prewarmWithCompletion:` ← `PRSServer prewarmWithCompletion:` (XPC from
SpringBoard). So the very first poster ("switcher") configuration can't be built —
most likely no poster extension/descriptor was found. Next: disassemble around +2040
for the assert text.
**Disassembly (owner's `ipsw dyld disass --vaddr 0x1c1777a70`):** the method starts with
`ldr x8, [self, #0x98]; cbnz x8, → assert` — the brk at +2040 is the "failure in
%{public}@ of <%{public}@:%p> (%{public}@:%i)" path, line 0xd67 (3431), i.e. an
assertion that the switcher configuration ivar (+0x98) is still nil. A second path
(+0x7b4, line 3432) asserts on byte ivar +0xe9. So PosterBoard is asked to set up the
first switcher configuration **twice in one process** (the first run stores it at
+0x98). Hypothesis: SpringBoard's `prewarm` XPC is re-sent (slow, CPU-rendered boot)
or the first-run state is not persisted, so the "first time" branch is taken again.
Assertion text is CFString `0x1e11644a0` (not yet read; a `grep -a` for
"witcherConfiguration" over the cache only finds selector/ivar names: `_switcherConfiguration`,
`_stateLock_initializeEmptySwitcherConfiguration`, `_stateLock_buildSwitcherConfigurationWithContext:outMutated:`,
`switcherConfigurationPath`, XPC entitlements `com.apple.posterboardservices.data-store.{access,mutate}SwitcherConfiguration`).
Next: disassemble the caller in `_stateLock_updateExtensions:…` before +5308 to see
what makes it call the first-time setup again.
**Caller (0x1c177e2c4..0x1c177e308):** `if (!self->_e9) { [self
_stateLock_setupSwitcherConfigurationForFirstTime]; self->_e9 = 1; } else if (added ||
deleted) [self _stateLock_transitionToNewSwitcherConfigurationAfterAddedExtensions:…]`.
So the first-time setup runs only while flag +0xe9 is clear, and it asserts because
`_switcherConfiguration` (+0x98) is **already non-nil on that first call** — something
else set it earlier in the same process without setting +0xe9 (candidate:
`_stateLock_initializeEmptySwitcherConfiguration` or a load of a persisted configuration).
Not the keybag: the AKS stub reports lock state 0x4 = unlocked since boot (patch 0038).
Cheap next check: pull the log of a later boot to see whether PosterBoard still crashes
once its data store has been written by the first run.
**Test 6 log (2026-10-23):** PosterBoard still crashes at the same place on a later boot
(`sub 0x1d7440268 - slide 0x15cc8000 = 0x1c1778268`, the brk after the line-3431 log) and
launchd respawns it. The test5 log (first iOS boot) shows the same crash on its first exec.
Disassembly confirms the entry check: `ldr x8,[self,#0x98]; cbnz x8 → assert` (before any
work), `ldrb [self,#0xe9]; cbnz → line 3432 assert`. The function itself sets +0x98 from
`_stateLock_buildSwitcherConfigurationWithContext:outMutated:`. So +0x98 is set before the
first call in every process: most likely loaded at init from a switcher configuration
already on disk (shipped in the root image's /var, or written by the first run).
Next (read-only, on the PC): loop-mount `ios16-root.img` (HFS+) read-only and look for
PosterBoard / PosterBoardServices data under /private/var/mobile/Library.
**Result:** the image has no PosterBoard data (only Weather's `WeatherPoster.appex` in
/private/var/staged_system_apps), so nothing persisted. **Root cause found (static):**
`-[PBFPosterExtensionDataStore _stateLock_switcherConfiguration]` (0x1c1778c10) is a lazy
getter: `if (!self->_switcherConfiguration) self->_switcherConfiguration =
[self _stateLock_buildSwitcherConfigurationWithContext:nil outMutated:nil];` and does NOT
set +0xe9. Callers include the public `-switcherConfiguration` (XPC from SpringBoard),
`_stateLock_activeConfiguration`, fetchPosterSnapshots, ingestPosterConfiguration,
applyUpdates, canPersistNewPosterConfiguration, … So any of those running before the
first `_stateLock_updateExtensions` pass builds the configuration, and the first-time setup
then asserts. On our CPU-only boot the order evidently differs from a real iPhone.
Fix candidate (needs approval, writes 4 bytes in userdata): NOP the entry check
`cbnz x8, assert` at cache vaddr **0x1c1777ab4** (`28 36 00 b5` → `1f 20 03 d5`), so the
first-time setup just rebuilds the configuration (it overwrites +0x98 and sets +0xe9).
Reversible by writing the 4 original bytes back. Unknown: whether XNU checks shared-cache
page hashes here (AMFI is permissive on this port).
**Applied 2026-10-23 ~19:07 (owner approved):** cache vaddr 0x1c1777ab4 = file
`dyld_shared_cache_arm64.19` offset 0x50cfab4 (`ipsw dyld a2o`) = **userdata byte offset
10261637812** (0x263a42ab4; unique 64-byte match in `ios16-root.img`, same bytes read back
from the phone first). Backup of the 4 KiB block (block 2505282) in the owner's
`~/ios-surya/backup/userdata-blk2505282-orig.bin`. Written from LineageOS recovery:
`printf '\037\040\003\325' | dd of=/dev/block/by-name/userdata bs=1 seek=10261637812 count=4 conv=notrunc`;
read-back shows `1f20 03d5`. Undo: same with `'\050\066\000\265'`. The PC's
`ios16-root.img` is still the unpatched original (reflashing it undoes the patch).
**Test 7 (patched):** kernel logs `CODE SIGNING: cs_invalid_page(...) p=…[PosterBoard] …
allowing (remove VALID) page` — modified shared-cache pages are accepted on this port.
PosterBoard now passes first-time setup and crashes later: `-[PBFWallpaperPublisher
activateWithSelectedLockPath:selectedHomePath:activeLockPath:activeHomePath:recentlyChanged:]
+836` ← `PBFPosterExtensionDataStoreXPCServiceGlue …didInitializeWithSwitcherConfiguration…`.
The brk is `NSParameterAssert(selectedLockPath)`: the switcher configuration has **no
poster at all** (no selected lock-screen poster). Likely real root cause: no poster
extension (Collections/Photos/Emoji/Gradient/…) delivered descriptors, so the first-time
setup builds an empty configuration (and the early lazy build was probably harmless).
Next: which extension/daemon processes run (ExtensionKit / pkd / poster extensions).
Checked (test 7 log + image): poster extensions are on the image (CollectionsPoster in
WallpaperKit, Emoji/Gradient/Pride/Unity/PhotosPosterProvider/Extragalactic/AegirPoster,
PaperBoard's LegacyPoster) but **none of them is ever exec'd**; `pkd` and `lsd` do run.
Upstream sweet has no wallpaper either. So PosterBoard's extension discovery returns no
provider (or it never asks before building the configuration). Without PosterBoard's
os_log output this is guesswork; parked for now (patch stays, harmless). Ideas for later:
route PosterBoard's os_log to the console (OS_ACTIVITY_DT_MODE / a libtrace hook), or
check the LaunchServices/ExtensionKit registration of `com.apple.posterkit.provider`.

## Backlight plan (research 2026-10-23)

- Hardware: live DT `qcom,wled@d800` on **PM6150L SID 5**, `qcom,pm6150l-spmi-wled`, ctrl
  `0xd800`, sink `0xd900`, string-cfg 0x07, fs-current 25 mA, CABC on. Mainline
  `drivers/video/backlight/qcom-wled.c`: `qcom,pm6150l-wled` = **WLED5**; brightness =
  15-bit LE at sink+0x53/0x54 (MOD_A) or +0x63/0x64 (MOD_B), then sync toggle at
  sink+0x65 (write 0, then bit0/bit1); module select per string at sink+0x73+n*0x10.
  Read SRC_SEL / brightness first to learn which module ABL uses.
- Kernel SPMI today (patch 0062 `sweet_spmi.c`) is read-only; need an EXT_WRITEL path
  (PMIC arbiter v5, `arb=0x50000000` in the log; opcode 0 in the channel CMD register,
  WDATA0 first) for APID of PPID 0x5d9 if APPS owns it.
- iOS side: backboardd looks for an IOService with `IOPropertyMatch backlight-control =
  true` and for `IODeviceTree:/backlight` (both "none" in `logs/test5.txt` lines 885/889).
  Next: dump the real D22AP `/backlight` node (owner's `research/ipsw-x/d22ap.adt`) and
  add a `SweetBacklight` service with those properties that logs every property set /
  user-client call (IOKit trace, patch 0049), then map the value to WLED brightness.
  Same service gives real screen-off (brightness 0) for the power button.

### Patch 0083 `surya-wled-backlight` (2026-10-23, untested)

- `bsd/dev/sweet_spmi.c`: init made race-safe (SweetBattery and SweetBacklight may start
  together); reads also allowed for SID 5 peripherals 0xd8/0xd9; new
  `sweet_spmi_write(sid, addr, val)`: EXT_WRITEL on chnls `0x0c600000 + 0x10000*APID`
  (WDATA0 +0x10, CMD +0x00 = addr<<4, STATUS +0x08), only for SID 5 registers 0xd846,
  0xd953/54, 0xd963/64, 0xd965 and only if the ownership table (cnfg+0x700+4*APID &7)
  gives the APID to EE 0 — same rules as Linux spmi-pmic-arb v5.
- `iokit/Kernel/arm/SweetBacklight.cpp` (IOResources personality, only with boot-arg
  `sweetbl=1`; `sweetbl=2` also dims to ~11% for 2 s at start as a visible self-test):
  reads WLED state at start and logs `SWEET BL: ctrl mod_en=… sink A en=… lvl=…`; picks
  the enabled module; publishes `backlight-control = true` and
  `IODisplayParameters.brightness {min 0, max 65536, value}`; logs every setProperties /
  newUserClient (`SWEET BL: …`) so backboardd's real protocol shows in the log; applies
  `brightness`/`IODisplayParameters.brightness(.value)` with a square curve (floor 0.5%).
- `SweetFramebuffer` RequestPowerChange → `sweet_backlight_display_power()`: clears /
  restores ctrl 0x46 bit 7 (WLED module enable), so screen off turns the LCD light off.
- Checked here: patch applies to the reconstructed post-0082 files; `clang -fsyntax-only`
  with stub headers (C and C++). Not built by CI yet.

**Test 8 (2026-10-23 ~22:40, CI run 40011481563, `ARGS="sweetbattery=1 sweetbl=2"`):**
`SWEET BL: ctrl mod_en=0x80 sink A en=0x80 w=0 lvl=0x7ff B en=0 … sync=0x3 src=0/0/0` →
module A, **12-bit** (max 0xfff), ABL leaves 0x7ff (50%). Self-test visibly dimmed the
screen (owner) → **SPMI writes work**. `SWEET FB: display off` → `SWEET BL: backlight off
(mod_en was 0x80)` and on again → **screen-off really turns the LCD light off**.
backboardd matched `backlight-control` → SweetBacklight, still looked up
`IODeviceTree:/backlight` (none), but **never called setProperties or newUserClient**, so
the slider does nothing. And the UI became **noticeably laggy**; the same kernel without
`sweetbl` was smooth again (A/B by the owner) → backboardd's brightness machinery costs
CPU once it finds a backlight service. Patch **0084**: `sweetbl` is a bit mask
(1 screen-off hook, 2 publish backlight-control [experimental], 4 self-test);
recommended `sweetbl=1`. Slider: needs the real protocol (likely the D22 `/backlight`
ADT node + whatever CoreBrightness reads) — later.

**Test 9 (2026-10-23 ~23:40, kernel with 0084, `ARGS="sweetbattery=1 sweetbl=1"`):** smooth
again and screen off is fully dark (owner: "it works"). Brightness slider in Control
Center / Settings moves but the panel stays the same (expected: no path from iOS to
SweetBacklight yet). The Control Center volume slider no longer reacts to dragging, but
the hardware volume keys still change the level and show the HUD (no audio hardware
anyway); probably a touch/gesture issue in that module, low priority.

## Audio plan (research 2026-10-23)

Facts from the live DT (`surya-dump/live.dts`):
- Sound card `sm6150-wcd9375-snd-card` (`qcom,sm6150-asoc-snd`): Bolero digital codec
  (rx/tx/va macros) + WCD9375 (headphone/mic), everything through the **ADSP** (q6 AFE/APR;
  `qcom,lpass@62400000` = `pil-tz-generic`, pas-id 1, firmware "adsp").
- Speakers: **two TI TAS256x** (`ti,tas256x`, left 0x4d, right 0x4c, stereo) on
  `i2c@0xa8c000` = **QUP1 SE3** (global SE9), pins gpio46/47 func qup13 (NORTH),
  reset PM6150L GPIO8 (active low, DT default output-low) and TLMM gpio91, irq gpio58/30.
  Audio data over **tertiary MI2S** (gpio53-56, func ter_mi2s, WEST), msm-mi2s-master.
- Haptics: awinic aw8624 (not audio). fsa4480 USB-C audio switch at 0x43 on the same bus.
- GCC: QUP1 S3 CBCR 0x183a4, RCG 0x183a8, vote 0x5200c bits 18-21 + 25.

Plan: milestone 1 = a beep from the shim without the ADSP (TAS256x over I2C + LPASS
LPAIF MI2S/DMA driven directly, as Linux does on sc7180 Chromebooks; **unknown** whether
LPASS is reachable from the apps CPU with the ADSP off). Then iOS's audio HAL side.
Step A1 (`SURYA_AUDIO=1 shim/build-surya.sh`, no payload): GCC/TLMM/PM6150L-GPIO8 reads,
QUP1 SE3 GENI I2C (copy of upstream's sweet touch I2C code), read TAS256x page-0 regs
0x00-0x07 and 0x7d at 0x4c/0x4d (pointer writes + reads only). LPASS not touched yet.

## Open questions (in order)

1. ~~Live DT of this phone~~ — done 2026-10-23, see above.
2. ~~Does ABL leave the panel in video mode?~~ — DT says video mode + cont-splash; confirm on screen with the shim.
2b. ~~Splash pixel format and stride~~ — a8r8g8b8, stride 4320 confirmed by Test 3.
3. Is the `spi0` serial engine already loaded in SPI mode at handoff (as sweet
   relied on ABL-loaded I²C firmware)?
4. Does the Novatek touch IC need host-uploaded firmware before it reports touches?
5. Stock `boot.img` header offsets for `shim/build.sh`.

## Plan

1. **Hardware map (read-only).** Dump live DT, `/proc/iomem`, stock `boot.img`
   from recovery. No writes to the phone.
2. **Config.** Surya values in `tools/mkadt.py`, `shim/console.h`, `shim/mmu.h`,
   `shim/shim.c`, `shim/build.sh`, early-fb patches 0010/0012.
3. **Shim-only boot** via `fastboot boot` (nothing flashed): shim text on screen
   confirms the framebuffer.
4. **Kernel boot** `cpus=1 sweettouch=0 sweetkeys=0`: log scroll confirms memory,
   IRQs, timer; UFS probe.
5. **iOS root** — only with explicit approval (erases `userdata`).
6. **Buttons**, then **Novatek SPI touch**.
7. Everything else (Wi-Fi, audio, GPU, …) is unsupported, as on sweet.

## Next action

Test 5 needs explicit owner approval (erases userdata = Ubuntu Touch).

## Previous next action

Test 2: read-only backup of `persist`, `modemst1/2`, `fsg`, `fsc`, `boot`,
`dtbo`, `vbmeta` from Ubuntu Touch, and get the `boot.img` header + DTB for
`shim/build.sh`.
