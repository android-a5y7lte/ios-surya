#!/bin/bash
# Test 4 on surya: pack the CI kernel + a generated ADT into the shim boot image.
# Kernel-only bring-up: 1 CPU, no touch/key drivers, NO UFS writes.
#
# Run on the dev PC from the repo root:
#   tools/surya-test4.sh path/to/kernel.development.sweet        # Test 4
#   tools/surya-test4.sh path/to/kernel.development.sweet 4b     # Test 4b
#   tools/surya-test4.sh path/to/kernel.development.sweet 5      # Test 5
# Test 5 = first iOS boot from the flashed root in userdata (README boot-args,
# but cpus=1 and sweettouch=0: surya's touch is Novatek SPI, not the Goodix
# I2C chip patch 0060 drives). It WRITES to userdata (ufsrw=1 rootrw=1) and
# keeps the persistent log ring at its end — only boot it after userdata holds
# the iOS root image. Put research/ipsw-x/d22ap.adt in place first so the ADT
# carries the iPhone X /product identity.
# Test 4b adds rd=ufsroot0: the kernel's own UFS driver opens `userdata`
# read-only and tries to mount it as the HFS root (expected to fail on the
# current ext4 userdata -> root mount panic). Still no ufsrw: the driver
# returns EROFS for every write.
# CPUS=N (env, default 6 since 2026-10-23: tested on surya) sets cpus=N; cores 0-5 are the Cortex-A55s (MPIDR
# 0x000-0x500 on surya, same as sweet), so CPUS=6 matches upstream's setting.
# CPUS=8 also starts the two Cortex-A76s (MPIDR 0x600/0x700; patch 0078).
# CPUS other than 1 adds "-cpuN" to the output name (CPUS=1 is the safe fallback).
# Needs: python3, clang, lld, llvm (llvm-objcopy/readelf/nm), and
#        dtb/surya/surya-boot.dtb (this phone's boot.img DTB, gitignored).
# Output: shim/out/shim-boot-surya-test4.img  -> fastboot boot it (nothing flashed).
#
# Safety: no ufsrw=1, so the kernel's UFS driver refuses every write
# (su.writable = 0); sweetlog=0 also disables the log ring at the end of
# userdata. Without rd=ufsroot0 the kernel stops at "waiting for root".
set -euo pipefail
cd "$(dirname "$0")/.."
KERNEL=${1:?usage: tools/surya-test4.sh KERNEL [4b]}
VARIANT=${2:-4}
CPUS=${CPUS:-6}
FBHZ=${FBHZ:-30}   # panel copy rate (sweetfb_hz); 60 = every vsync, smoother, more CPU
case "$CPUS" in [1-8]) ;; *) echo "error: CPUS must be 1..8" >&2; exit 1 ;; esac
NAME=test$VARIANT; [ "$CPUS" = 1 ] || NAME=$NAME-cpu$CPUS; [ "$FBHZ" = 30 ] || NAME=$NAME-hz$FBHZ
case "$VARIANT" in
	4) EXTRA='' ;;
	4b) EXTRA=' rd=ufsroot0' ;;
	5) EXTRA=' rd=ufsroot0 ufsrw=1 rootrw=1 unentitled_ios_sim_launch=1 ignition_level=1 launchd_unsecure_cache=1 sweetfb_noconsole=1 sweetfb_hz='"$FBHZ" ;;
	*) echo "error: variant must be 4, 4b or 5" >&2; exit 1 ;;
esac
# Tests 4/4b: buttons off and no log ring (nothing may write). Test 5: the
# button driver only reads PMIC registers and surya's keys sit on the same
# PM6150/PM6150L pins as sweet's, so it stays on; the log ring is allowed.
TOUCH='sweettouch=0'; BLOB=()
FW=research/fw/novatek_ts_huaxing_fw.bin
if [ "$VARIANT" = 5 ]; then
	KEYSLOG=''
	# Touch (patch 0082, needs a kernel built with it): firmware as a payload blob.
	if [ -f "$FW" ] && [ "${SURYA_TOUCH:-1}" = 1 ]; then
		TOUCH='sweettouch=2'; BLOB=(--blob "TouchFW=$FW")
	else
		echo "note: $FW missing (or SURYA_TOUCH=0): touch disabled" >&2
	fi
	[ -f research/ipsw-x/d22ap.adt ] || echo "warning: research/ipsw-x/d22ap.adt missing: iOS will see an iPod with a Home button" >&2
else
	KEYSLOG='sweetkeys=0 sweetlog=0'
fi
[ -f dtb/surya/surya-boot.dtb ] || { echo "error: dtb/surya/surya-boot.dtb missing" >&2; exit 1; }
mkdir -p shim/out
python3 tools/mkadt.py shim/out/surya.adt --highmem
python3 tools/mkpayload.py "$KERNEL" shim/out/surya.adt shim/out/payload-surya-$NAME.bin \
	"${BLOB[@]}" \
	--cmdline "debug=0xc serial=3 cpus=$CPUS keepsyms=1 -enable_kprintf_spam -noprogress wdt=-1 wdt_test=no_user_checkin $TOUCH $KEYSLOG$EXTRA"
shim/build-surya.sh shim/out/payload-surya-$NAME.bin
mv shim/out/shim-boot-surya.img shim/out/shim-boot-surya-$NAME.img
sha256sum shim/out/shim-boot-surya-$NAME.img
