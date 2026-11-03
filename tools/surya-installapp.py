#!/usr/bin/env python3
"""Install an app bundle onto the phone so it shows up on the home screen.

  tools/surya-installapp.py APP           a .app directory or an .ipa file
  tools/surya-installapp.py --list        list the apps in /Applications on the phone
  tools/surya-installapp.py --remove NAME  remove /Applications/NAME.app

No App Store and no Apple sign-in. The kernel runs unsigned code (our AMFI grants
platform-binary to everything on the root filesystem, patches 0025/0035), so an app
is just its .app bundle copied into /Applications and registered with LaunchServices
- the same route a jailbroken device uses. Apps that need Apple's online services
(activation, iCloud, the App Store itself) still fail at runtime; self-contained apps run.

What it does (over SSH/scp, phone up with the USB link):
  - .ipa: unzip here on the PC, take Payload/<name>.app (no unzip needed on the phone)
  - read CFBundleExecutable from the bundle's Info.plist (text or binary plist)
  - scp the bundle to /Applications/<name>.app, chown root:wheel, chmod 755 the executable
  - register it: `uicache` if the phone has it; otherwise prod LaunchServices by
    restarting lsd + SpringBoard. If it still does not appear, reboot once with
    sweetlsreset=1 (patch 0098) to force a LaunchServices rebuild.
"""
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
import zipfile

PHONE = "root@192.168.7.2"


def ssh(command, check=True, capture=False):
    """Run one shell command on the phone (passed as a single string)."""
    print("+ ssh", command)
    r = subprocess.run(["ssh", PHONE, command], check=check, text=True,
                       stdout=subprocess.PIPE if capture else None)
    return r.stdout if capture else None


def find_app(path, workdir):
    """Return a path to a .app directory, unpacking an .ipa if needed."""
    if path.endswith(".app") and os.path.isdir(path):
        return path
    if path.endswith(".ipa") and os.path.isfile(path):
        with zipfile.ZipFile(path) as z:
            z.extractall(workdir)
        payload = os.path.join(workdir, "Payload")
        apps = [a for a in os.listdir(payload)] if os.path.isdir(payload) else []
        apps = [os.path.join(payload, a) for a in apps if a.endswith(".app")]
        if not apps:
            sys.exit(f"{path}: no Payload/*.app inside the .ipa")
        return apps[0]
    sys.exit(f"{path}: not a .app directory or an .ipa file")


def bundle_exe(appdir):
    info = os.path.join(appdir, "Info.plist")
    if not os.path.isfile(info):
        sys.exit(f"{appdir}: no Info.plist (not an app bundle)")
    with open(info, "rb") as f:
        d = plistlib.load(f)
    exe = d.get("CFBundleExecutable")
    if not exe:
        sys.exit(f"{appdir}: Info.plist has no CFBundleExecutable")
    return exe


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)

    if args[0] == "--list":
        ssh("ls -1 /Applications | grep '\\.app$' || true")
        return
    if args[0] == "--remove":
        if len(args) != 2:
            sys.exit("usage: --remove NAME  (NAME without .app)")
        name = args[1][:-4] if args[1].endswith(".app") else args[1]
        ssh(f"rm -rf /Applications/{name}.app && echo removed /Applications/{name}.app")
        print("removed; respring (kill SpringBoard) or reboot to update the home screen")
        return

    src = args[0]
    with tempfile.TemporaryDirectory() as work:
        appdir = find_app(src, work)
        name = os.path.splitext(os.path.basename(appdir))[0]
        exe = bundle_exe(appdir)
        print(f"app: {name}.app  executable: {exe}")

        dest = f"/Applications/{name}.app"
        ssh(f"rm -rf {dest}")
        print(f"+ scp -rq {appdir} -> {PHONE}:{dest}")
        subprocess.run(["scp", "-rq", appdir, f"{PHONE}:{dest}"], check=True)
        ssh(f"chown -R root:wheel '{dest}' && chmod 755 '{dest}/{exe}'")

        # register with LaunchServices
        out = ssh("command -v uicache || true", capture=True) or ""
        if out.strip():
            ssh(f"uicache -p '{dest}'")
            print("registered with uicache; the icon should appear shortly")
        else:
            print("no uicache on the phone; prodding LaunchServices (restart lsd + SpringBoard)")
            ssh("kill -9 $(ps -A -o pid,comm | awk '/\\/usr\\/libexec\\/lsd$/{print $1}') 2>/dev/null; "
                "kill -9 $(ps -A -o pid,comm | awk '/SpringBoard$/{print $1}') 2>/dev/null; true")
            print("if the icon does not appear in ~1 min, reboot once with sweetlsreset=1 "
                  "(forces a LaunchServices rebuild, patch 0098)")


if __name__ == "__main__":
    main()
