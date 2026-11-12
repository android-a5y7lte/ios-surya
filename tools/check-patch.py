#!/usr/bin/env python3
"""Lint (and optionally repair) a unified-diff patch before it reaches CI.

Two failure modes have bitten this project's hand-written xnu patches:

  1. A blank context line inside a hunk must be a single space (" \n"), but
     editors and generators strip the trailing space, leaving a bare "\n".
     git apply reads that as end-of-patch and reports "corrupt patch at line N".

  2. The @@ -old_start,old_count +new_start,new_count @@ counts must equal
     ctx+minus and ctx+plus respectively. Miscounting them makes git apply
     silently SKIP the whole patch (the build then succeeds with none of the
     intended changes, which is far worse than a hard failure).

Usage:
    tools/check-patch.py xnu/patches/0128-foo.patch            # report only
    tools/check-patch.py --fix xnu/patches/0128-foo.patch      # repair in place

--fix restores stripped blank-context spaces and rewrites the @@ counts to
match the body. It never touches the start line numbers; those must be right
for the tree the patch targets. Verify with:
    cd /path/to/xnu && git apply --check /path/to/patch
"""

import argparse
import re
import sys
from pathlib import Path

HUNK_RE = re.compile(rb"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$")
FILE_HDR = (b"diff ", b"--- ", b"+++ ", b"index ", b"new file", b"deleted file")


def is_file_header(line: bytes) -> bool:
    return any(line.startswith(p) for p in FILE_HDR)


def lint(path: Path, fix: bool) -> int:
    raw = path.read_bytes()
    # Keep a trailing-newline flag so we can restore it exactly.
    trailing_nl = raw.endswith(b"\n")
    lines = raw.split(b"\n")
    if trailing_nl and lines and lines[-1] == b"":
        lines.pop()

    out: list[bytes] = []
    problems = 0
    repaired_blanks = 0
    rewritten_counts = 0

    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        m = HUNK_RE.match(line)
        if not m:
            out.append(line)
            i += 1
            continue

        old_s = int(m.group(1))
        old_c = int(m.group(2)) if m.group(2) is not None else 1
        new_s = int(m.group(3))
        new_c = int(m.group(4)) if m.group(4) is not None else 1
        tail = m.group(5)
        header_lineno = i + 1

        # Collect the hunk body: everything until the next @@ or a file header.
        body_start = i + 1
        j = body_start
        body: list[bytes] = []
        while j < n and not HUNK_RE.match(lines[j]) and not is_file_header(lines[j]):
            body.append(lines[j])
            j += 1

        # Trailing bare-empty lines at EOF are the file's final newline, not
        # part of the hunk. Drop them from the body (and keep them verbatim).
        trailer: list[bytes] = []
        while body and body[-1] == b"" and j >= n:
            trailer.insert(0, body.pop())

        ctx = plus = minus = 0
        fixed_body: list[bytes] = []
        for off, b in enumerate(body):
            if b == b"":
                # Blank line inside a hunk == blank CONTEXT line, needs a space.
                problems += 1
                if fix:
                    b = b" "
                    repaired_blanks += 1
                else:
                    print(
                        f"{path.name}:{header_lineno + 1 + off}: "
                        f"bare empty line inside hunk (needs a leading space)"
                    )
            c = b[:1]
            if c == b"+":
                plus += 1
            elif c == b"-":
                minus += 1
            elif c == b" ":
                ctx += 1
            elif c == b"\\":
                pass  # "\ No newline at end of file"
            else:
                problems += 1
                print(
                    f"{path.name}:{header_lineno + 1 + off}: "
                    f"line starts with {c!r}, expected ' ', '+', '-' or '\\'"
                )
            fixed_body.append(b)

        obs_old = ctx + minus
        obs_new = ctx + plus
        ok_old = obs_old == old_c
        ok_new = obs_new == new_c

        if not ok_old or not ok_new:
            problems += 1
            msg = (
                f"{path.name}:{header_lineno}: @@ -{old_s},{old_c} +{new_s},{new_c} @@ "
                f"declares old={old_c} new={new_c} but body has "
                f"ctx={ctx} -={minus} +={plus} => old={obs_old} new={obs_new}"
            )
            if fix:
                old_c, new_c = obs_old, obs_new
                rewritten_counts += 1
                print(msg + "  [FIXED]")
            else:
                print(msg)
        else:
            print(
                f"{path.name}:{header_lineno}: @@ -{old_s},{old_c} +{new_s},{new_c} @@  "
                f"ctx={ctx} -={minus} +={plus}  OK"
            )

        out.append(b"@@ -%d,%d +%d,%d @@%s" % (old_s, old_c, new_s, new_c, tail))
        out.extend(fixed_body)
        out.extend(trailer)
        i = j

    if fix and (repaired_blanks or rewritten_counts):
        blob = b"\n".join(out)
        if trailing_nl:
            blob += b"\n"
        path.write_bytes(blob)
        print(
            f"\n{path.name}: repaired {repaired_blanks} blank-context line(s), "
            f"rewrote {rewritten_counts} hunk count(s)"
        )
        return 0

    if problems:
        print(f"\n{path.name}: {problems} problem(s). Re-run with --fix to repair.")
        return 1

    print(f"\n{path.name}: clean")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("patch", nargs="+", type=Path)
    ap.add_argument("--fix", action="store_true",
                    help="repair blank-context lines and @@ counts in place")
    a = ap.parse_args()
    rc = 0
    for p in a.patch:
        if not p.is_file():
            print(f"{p}: not a file", file=sys.stderr)
            rc = 1
            continue
        rc |= lint(p, a.fix)
    return rc


if __name__ == "__main__":
    sys.exit(main())
