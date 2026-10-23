#!/bin/bash
# Test 4 on surya: pack the CI kernel + a generated ADT into the shim boot image.
# Kernel-only bring-up: 1 CPU, no touch/key drivers, NO UFS writes.
#
# Run on the dev PC from the repo root:
#   tools/surya-test4.sh path/to/kernel.development.sweet        # Test 4
#   tools/surya-test4.sh path/to/kernel.development.sweet 4b     # Test 4b
# Test 4b adds rd=ufsroot0: the kernel's own UFS driver opens `userdata`
# read-only and tries to mount it as the HFS root (expected to fail on the
# current ext4 userdata -> root mount panic). Still no ufsrw: the driver
# returns EROFS for every write.
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
case "$VARIANT" in
	4) EXTRA='' ;;
	4b) EXTRA=' rd=ufsroot0' ;;
	*) echo "error: variant must be 4 or 4b" >&2; exit 1 ;;
esac
[ -f dtb/surya/surya-boot.dtb ] || { echo "error: dtb/surya/surya-boot.dtb missing" >&2; exit 1; }
mkdir -p shim/out
python3 tools/mkadt.py shim/out/surya.adt --highmem
python3 tools/mkpayload.py "$KERNEL" shim/out/surya.adt shim/out/payload-surya-test$VARIANT.bin \
	--cmdline "debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress wdt=-1 wdt_test=no_user_checkin sweettouch=0 sweetkeys=0 sweetlog=0$EXTRA"
shim/build-surya.sh ../shim/out/payload-surya-test$VARIANT.bin
mv shim/out/shim-boot-surya.img shim/out/shim-boot-surya-test$VARIANT.img
sha256sum shim/out/shim-boot-surya-test$VARIANT.img
