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

## Verified: sweet vs surya

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

## Open questions (in order)

1. Live DT of *this* phone: framebuffer address, memory map, reserved carve-outs.
2. Does ABL leave the Huaxing panel running in video mode (no kick needed)?
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

Step 1: get the live device tree from the phone (owner runs it; waiting).
