#!/usr/bin/env python3
"""Install a jailbreak package manager ("store") by laying down a Procursus bootstrap.

  tools/surya-bootstrap.py BOOTSTRAP.tar[.zst|.xz]            install the bootstrap at /var/jb
  tools/surya-bootstrap.py BOOTSTRAP.tar.zst --tar ./tar      also push a static tar first
  tools/surya-bootstrap.py --list                             list /var/jb on the phone
  tools/surya-bootstrap.py --no-prep BOOTSTRAP.tar.zst        extract only, skip the prep step

Our port is already a jailbreak environment (the kernel runs unsigned code; AMFI grants
platform-binary to the root FS), so the browsable "stores" that fit are the jailbreak
package managers - Sileo (default on iOS 16), Zebra, Cydia - which install .deb packages
from APT repos on top of a bootstrap. The iOS-16 standard (as used by Dopamine) is a
Procursus ROOTLESS bootstrap at /var/jb + Sileo. See SURYA_NOTES.md '"Different stores"'.

NOT handled here (don't need them on this port): Apple App Store (device identity),
AltStore/SideStore (Apple cert + refresh daemon, for non-jailbroken devices), TrollStore
(a CoreTrust signing exploit - we already run unsigned). Single .ipa apps: surya-installapp.

What this does, over SSH (phone up on the USB link):
  - decompress .zst/.xz to a plain .tar on the PC (the phone has no zstd/xz yet);
  - iOS ships NO tar, and a bootstrap is full of symlinks + setuid bins, so `scp -r` would
    wreck it. This pushes the .tar and extracts it WITH A tar BINARY on the phone,
    preserving perms/symlinks. Use --tar to push a static tar/bsdtar first if the phone
    has none (none of ours do until the bootstrap lands);
  - mkdir /var/jb, extract the tar there (-xpf, preserve), run the Procursus prep
    (prep_bootstrap.sh), then uicache -a to register Sileo/Zebra on the home screen.

UNTESTED on the phone - the prep postinsts assume a jailbreakd-ish environment and may
need per-step tweaks. The tool prints each remote command so you can also run them by hand.
Needs the bootstrap tarball fetched at the PC (the sandbox here blocks the download).
"""
import os
import subprocess
import sys

PHONE = "root@192.168.7.2"
JBROOT = "/var/jb"


def ssh(cmd, check=True):
    print("+ ssh", cmd)
    return subprocess.run(["ssh", PHONE, cmd], check=check).returncode


def scp(local, remote):
    print(f"+ scp {local} -> {PHONE}:{remote}")
    subprocess.run(["scp", "-q", local, f"{PHONE}:{remote}"], check=True)


def decompress(path):
    """Return a path to a plain .tar, decompressing .zst/.xz on the PC if needed."""
    if path.endswith(".tar"):
        return path
    out = path.rsplit(".", 1)[0]
    if not out.endswith(".tar"):
        out += ".tar"
    if path.endswith(".zst"):
        tool = ["zstd", "-d", "-f", path, "-o", out]
    elif path.endswith((".xz", ".lzma")):
        tool = ["xz", "-dkf", path]  # writes path without .xz
        out = path[:-3] if path.endswith(".xz") else path[:-5]
    else:
        sys.exit(f"{path}: not .tar/.tar.zst/.tar.xz")
    print("+", " ".join(tool))
    subprocess.run(tool, check=True)
    if not os.path.isfile(out):
        sys.exit(f"decompression did not produce {out}")
    return out


def have_remote_tar():
    for t in (f"{JBROOT}/usr/bin/tar", "/usr/bin/tar", "/var/tmp/tar"):
        if ssh(f"test -x {t}", check=False) == 0:
            return t
    return None


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    if args[0] == "--list":
        ssh(f"ls -la {JBROOT} 2>/dev/null || echo 'no {JBROOT} yet'", check=False)
        return

    no_prep = "--no-prep" in args
    args = [a for a in args if a != "--no-prep"]
    static_tar = None
    if "--tar" in args:
        i = args.index("--tar")
        static_tar = args[i + 1]
        del args[i:i + 2]
    if not args:
        sys.exit("need a bootstrap tarball")
    tarball = args[0]
    if not os.path.isfile(tarball):
        sys.exit(f"{tarball}: no such file")

    plain = decompress(tarball)

    # make sure the phone can extract (preserving symlinks + setuid)
    if static_tar:
        scp(static_tar, "/var/tmp/tar")
        ssh("chmod 755 /var/tmp/tar")
    tar = have_remote_tar()
    if not tar:
        sys.exit("the phone has no tar binary. Re-run with --tar ./path-to-static-tar "
                 "(a static arm64 tar/bsdtar). scp -r cannot be used: it breaks symlinks "
                 "and setuid bits a bootstrap needs.")
    print(f"using remote tar: {tar}")

    ssh(f"mkdir -p {JBROOT}")
    scp(plain, "/var/tmp/bootstrap.tar")
    ssh(f"{tar} -xpf /var/tmp/bootstrap.tar -C {JBROOT}")
    ssh(f"chown -R root:wheel {JBROOT}")

    if no_prep:
        print("extracted only (--no-prep). Prep later: "
              f"ssh {PHONE} '{JBROOT}/prep_bootstrap.sh'")
        return

    # Procursus prep: sets up dpkg/apt/profile.d + runs base postinsts
    if ssh(f"test -x {JBROOT}/prep_bootstrap.sh", check=False) == 0:
        ssh(f"{JBROOT}/prep_bootstrap.sh")
    else:
        print(f"no {JBROOT}/prep_bootstrap.sh in this bootstrap - run its documented prep "
              "manually (dpkg --configure -a inside the jbroot, then the postinsts)")

    # register the manager apps on the home screen
    if ssh(f"test -x {JBROOT}/usr/bin/uicache", check=False) == 0:
        ssh(f"{JBROOT}/usr/bin/uicache -a", check=False)
    print("done (best-effort). Open Sileo/Zebra on the phone; if the icon is missing, "
          "reboot once with sweetlsreset=1. Then add repos and install from there.")


if __name__ == "__main__":
    main()
