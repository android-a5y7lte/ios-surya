#!/bin/bash
# Build the shim for the POCO X3 NFC (surya) with clang/lld (no GNU cross
# toolchain needed) and pack it with tools/mkbootimg_v2.py.
#
# Input: dtb/surya/surya-boot.dtb = the DTB section of this phone's own boot
# partition (gitignored). surya's stock boot header uses the same offsets as
# sweet (kernel 0x8000, ramdisk 0x1000000, tags 0x100, dtb 0x1f00000, page
# 4096, v2), so only the DTB differs from build.sh.
#
# Usage: shim/build-surya.sh [payload.bin]   -> shim/out/shim-boot-surya.img
#        (no payload = empty ramdisk = shim diagnostics only)
# Touch bring-up: SURYA_TOUCH_STEP=1 (3a probe, default), 2 (3b chip ID),
#        3 (3c firmware download + touch dots; pass novatek_ts_huaxing_fw.bin
#        as the 'payload' so it becomes the ramdisk).
set -euo pipefail
# Resolve the optional payload path before changing directory.
PAYLOAD=${1:+$(realpath "$1")}
cd "$(dirname "$0")"
DTB=../dtb/surya/surya-boot.dtb
[ -f "$DTB" ] || { echo "error: $DTB missing (see SURYA_NOTES.md)" >&2; exit 1; }
CC="clang --target=aarch64-linux-gnu"
CFLAGS="-O2 -ffreestanding -nostdlib -fno-pic -fno-jump-tables -fno-builtin-strcmp \
 -mgeneral-regs-only -mstrict-align -fno-stack-protector -Wall -Wextra -DSURYA_TOUCH_PROBE -DSURYA_TOUCH_STEP=${SURYA_TOUCH_STEP:-1}"
# Audio bring-up: SURYA_AUDIO=1 runs the speaker amp probe (surya_audio.c)
# instead of the touch probe; use it without a payload (shim diagnostics).
# SURYA_AUDIO=2 also releases both amp resets (step A2); 3 adds the LPASS probe (A3).
case "${SURYA_AUDIO:-0}" in 1|2|3) CFLAGS="$CFLAGS -DSURYA_AUDIO_PROBE -DSURYA_AUDIO_STEP=$SURYA_AUDIO" ;; esac
OBJS="start shim console mmu fdt loader ufs compat surya_touch surya_audio"
mkdir -p out/surya
for f in start.S shim.c console.c mmu.c fdt.c loader.c ufs.c compat.c surya_touch.c surya_audio.c; do
	$CC $CFLAGS -c $f -o out/surya/${f%.*}.o
done
# Same rule as build.sh: no absolute relocations outside start.o.
abs=$(llvm-readelf -r $(for o in $OBJS; do [ $o = start ] || echo out/surya/$o.o; done) | grep -c ABS64 || true)
[ "$abs" = 0 ] || { echo "error: $abs absolute relocations in C objects" >&2; exit 1; }
ld.lld -static -T shim.ld --build-id=none -N $(for o in $OBJS; do echo out/surya/$o.o; done) -o out/surya/shim.elf
llvm-objcopy -O binary out/surya/shim.elf out/surya/Image
size=$((16#$(llvm-nm out/surya/shim.elf | awk '$3=="_end"{print $1}')))  # no gawk strtonum (Mint ships mawk)
truncate -s "$size" out/surya/Image
if [ -n "$PAYLOAD" ]; then
	cp "$PAYLOAD" out/surya/ramdisk.bin
else
	python3 ../tools/mkbootimg_v2.py --empty-ramdisk out/surya/ramdisk.bin
fi
python3 ../tools/mkbootimg_v2.py --kernel out/surya/Image --ramdisk out/surya/ramdisk.bin \
	--dtb "$DTB" --base 0 --kernel_offset 0x8000 --ramdisk_offset 0x1000000 \
	--tags_offset 0x100 --dtb_offset 0x1f00000 --pagesize 4096 \
	--os_version 17.0.0 --os_patch_level 2035-06 --cmdline "console=null" \
	-o out/shim-boot-surya.img
ls -l out/surya/Image out/shim-boot-surya.img
