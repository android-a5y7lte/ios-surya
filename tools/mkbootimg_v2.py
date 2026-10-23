#!/usr/bin/env python3
"""Minimal Android boot image v2 packer (no AOSP mkbootimg needed).

Writes the same layout as `mkbootimg --header_version 2`: header page, kernel,
ramdisk, (no second, no recovery dtbo), dtb, each padded to the page size.
Used by shim/build-surya.sh. Usage:

  mkbootimg_v2.py --kernel K --ramdisk R --dtb D -o OUT [--base 0]
      [--kernel_offset 0x8000] [--ramdisk_offset 0x1000000] [--tags_offset 0x100]
      [--dtb_offset 0x1f00000] [--pagesize 4096] [--cmdline STR]
      [--os_version 17.0.0] [--os_patch_level 2035-06]
  mkbootimg_v2.py --empty-ramdisk OUT    # gzipped newc cpio with only "."
"""
import argparse, gzip, hashlib, struct, sys

HDR_V2_SIZE = 1660


def os_version_field(ver, patch):
    a, b, c = (int(x) for x in ver.split('.'))
    y, m = (int(x) for x in patch.split('-'))
    return ((a << 14) | (b << 7) | c) << 11 | ((y - 2000) << 4) | m


def pad(data, page):
    return data + b'\0' * (-len(data) % page)


def empty_cpio():
    def entry(name, mode, nlink):
        n = name.encode() + b'\0'
        hdr = ('070701' + '%08x' * 13) % (0, mode, 0, 0, nlink, 0, 0, 0, 0, 0, 0, len(n), 0)
        out = hdr.encode() + n
        return out + b'\0' * (-len(out) % 4)
    data = entry('.', 0o40755, 2) + entry('TRAILER!!!', 0, 1)
    return gzip.compress(data + b'\0' * (-len(data) % 512), 9, mtime=0)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--empty-ramdisk':
        open(sys.argv[2], 'wb').write(empty_cpio())
        return
    p = argparse.ArgumentParser()
    num = lambda s: int(s, 0)
    p.add_argument('--kernel', required=True)
    p.add_argument('--ramdisk', required=True)
    p.add_argument('--dtb', required=True)
    p.add_argument('-o', '--output', required=True)
    p.add_argument('--base', type=num, default=0)
    p.add_argument('--kernel_offset', type=num, default=0x8000)
    p.add_argument('--ramdisk_offset', type=num, default=0x1000000)
    p.add_argument('--tags_offset', type=num, default=0x100)
    p.add_argument('--dtb_offset', type=num, default=0x1f00000)
    p.add_argument('--pagesize', type=num, default=4096)
    p.add_argument('--cmdline', default='')
    p.add_argument('--os_version', default='17.0.0')
    p.add_argument('--os_patch_level', default='2035-06')
    a = p.parse_args()

    kernel = open(a.kernel, 'rb').read()
    ramdisk = open(a.ramdisk, 'rb').read()
    dtb = open(a.dtb, 'rb').read()
    if dtb[:4] != b'\xd0\x0d\xfe\xed':
        sys.exit('error: %s is not a flattened device tree' % a.dtb)
    cmd = a.cmdline.encode()
    if len(cmd) > 511:
        sys.exit('error: cmdline longer than 511 bytes')

    sha = hashlib.sha1()
    for blob in (kernel, ramdisk, b''):            # kernel, ramdisk, second
        sha.update(blob); sha.update(struct.pack('<I', len(blob)))
    sha.update(b''); sha.update(struct.pack('<I', 0))  # recovery dtbo (v1+)
    sha.update(dtb); sha.update(struct.pack('<I', len(dtb)))  # dtb (v2)

    hdr = b'ANDROID!'
    hdr += struct.pack('<10I', len(kernel), a.base + a.kernel_offset,
                       len(ramdisk), a.base + a.ramdisk_offset,
                       0, 0, a.base + a.tags_offset, a.pagesize, 2,
                       os_version_field(a.os_version, a.os_patch_level))
    hdr += b'\0' * 16                               # name
    hdr += cmd.ljust(512, b'\0')                    # cmdline
    hdr += sha.digest().ljust(32, b'\0')            # id
    hdr += b'\0' * 1024                             # extra_cmdline
    hdr += struct.pack('<IQI', 0, 0, HDR_V2_SIZE)   # recovery dtbo, header size
    hdr += struct.pack('<IQ', len(dtb), a.base + a.dtb_offset)
    assert len(hdr) == HDR_V2_SIZE

    with open(a.output, 'wb') as f:
        for blob in (hdr, kernel, ramdisk, dtb):
            f.write(pad(blob, a.pagesize))


if __name__ == '__main__':
    main()
