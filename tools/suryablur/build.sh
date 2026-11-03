#!/bin/sh
# Build suryablur for the phone on Linux: clang + ld64.lld, .tbd stub, ad-hoc signed.
set -e
cd "$(dirname "$0")"
clang -nostdinc -ffreestanding -target arm64-apple-ios16.0 -O2 -c suryablur.c -o suryablur.o
LD64=$(command -v ld64.lld || ls /usr/lib/llvm-*/bin/ld64.lld | tail -1)
"$LD64" -arch arm64 -platform_version ios 16.0 16.0 -adhoc_codesign -o suryablur \
	suryablur.o stubs/libSystem.tbd
rm -f suryablur.o
echo "built $(pwd)/suryablur"
