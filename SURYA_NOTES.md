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

## Open questions (in order)

1. ~~Live DT of this phone~~ — done 2026-10-23, see above.
2. ~~Does ABL leave the panel in video mode?~~ — DT says video mode + cont-splash; confirm on screen with the shim.
2b. Splash pixel format and stride.
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

Test 2: read-only backup of `persist`, `modemst1/2`, `fsg`, `fsc`, `boot`,
`dtbo`, `vbmeta` from Ubuntu Touch, and get the `boot.img` header + DTB for
`shim/build.sh`.
