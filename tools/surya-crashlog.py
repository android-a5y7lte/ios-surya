#!/usr/bin/env python3
"""Pull and decode an app/process crash report from the phone, to see why it won't open.

  tools/surya-crashlog.py                list the recent crash reports on the phone
  tools/surya-crashlog.py NAME           decode the newest crash for NAME (e.g. MobileSafari, Maps)
  tools/surya-crashlog.py NAME -n 3      the newest 3 for NAME

iOS writes a .ips crash report for every process that crashes, under
/var/mobile/Library/Logs/CrashReporter/ (and .../Retired/). Each file is a JSON header
line followed by a JSON body (threads, usedImages, exception, termination). This copies
the file here and prints the exception, the termination reason, and the faulting thread's
frames resolved to image + symbol - the same read we did by hand for PosterBoard.

Reads only; nothing on the phone is changed. Phone up with the USB link.
"""
import json
import os
import subprocess
import sys
import tempfile

PHONE = "root@192.168.7.2"
CRASHDIR = "/var/mobile/Library/Logs/CrashReporter"


def ssh(command, capture=True):
    r = subprocess.run(["ssh", PHONE, command], text=True,
                       stdout=subprocess.PIPE if capture else None)
    return r.stdout or "" if capture else None


def list_reports():
    out = ssh(f"ls -t {CRASHDIR}/*.ips {CRASHDIR}/Retired/*.ips 2>/dev/null | head -40")
    names = [l.strip() for l in out.splitlines() if l.strip()]
    if not names:
        print("no crash reports on the phone")
        return
    print("recent crash reports (newest first):")
    for n in names:
        print("  ", os.path.basename(n))


def newest_for(name, count):
    out = ssh(f"ls -t {CRASHDIR}/{name}-*.ips {CRASHDIR}/Retired/{name}-*.ips 2>/dev/null | head -{count}")
    return [l.strip() for l in out.splitlines() if l.strip()]


def decode(remote_path, workdir):
    local = os.path.join(workdir, os.path.basename(remote_path))
    subprocess.run(["scp", "-q", f"{PHONE}:{remote_path}", local], check=True)
    txt = open(local, errors="replace").read()
    nl = txt.index("\n")
    header = json.loads(txt[:nl])
    body = json.loads(txt[nl + 1:])

    print("=" * 70)
    print(f"{header.get('name','?')}  (iOS {header.get('os_version','?')})  {os.path.basename(remote_path)}")
    exc = body.get("exception", {})
    print(f"exception: {exc.get('type','?')} {exc.get('signal','')} codes={exc.get('codes','')}")
    term = body.get("termination")
    if term:
        print(f"termination: {term.get('namespace','')} code {term.get('code','')} "
              f"{term.get('indicator','')} byProc={term.get('byProc','')}")
    asi = body.get("asi")
    if asi:
        print("asi:", json.dumps(asi)[:400])
    for l in body.get("lastExceptionBacktrace", [])[:1]:
        pass

    imgs = body.get("usedImages", [])
    ft = body.get("faultingThread", 0)
    threads = body.get("threads", [])

    def show_frames(frames, label):
        print(label)
        for f in frames[:18]:
            i = f.get("imageIndex", -1)
            img = imgs[i] if 0 <= i < len(imgs) else {}
            sym = f.get("symbol", hex(f.get("imageOffset", 0)))
            print(f"    {img.get('name','?'):22} {sym} +{f.get('symbolLocation','')}")

    leb = body.get("lastExceptionBacktrace")
    if leb:
        show_frames(leb, "last exception backtrace (the throw):")
    if ft < len(threads):
        t = threads[ft]
        show_frames(t.get("frames", []), f"faulting thread {ft} ({t.get('queue','')}):")


def main():
    args = sys.argv[1:]
    if not args:
        list_reports()
        return
    name = args[0]
    count = 1
    if "-n" in args:
        count = int(args[args.index("-n") + 1])
    reports = newest_for(name, count)
    if not reports:
        print(f"no crash reports for '{name}'. Run with no argument to list what's there.")
        print("tip: open the app on the phone, wait a few seconds, then run this again.")
        return
    with tempfile.TemporaryDirectory() as work:
        for r in reports:
            decode(r, work)


if __name__ == "__main__":
    main()
