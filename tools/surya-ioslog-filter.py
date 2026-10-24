#!/usr/bin/env python3
"""Cut the decoded iOS unified log down to the extension / wallpaper / widget story.

  tools/surya-ioslog-filter.py logs/ioslog.jsonl > logs/ioslog-ext.txt

Input: the JSONL written by Mandiant's unifiedlog_iterator
  (unifiedlog_iterator -m log-archive -i logs/ioslog.logarchive -o logs/ioslog.jsonl)
from /var/db/diagnostics + /var/db/uuidtext copied off the phone.

Keeps the lines from the processes that discover, approve and start app
extensions (lsd, pkd, runningboardd, extensionkitservice, PosterBoard, chronod,
SpringBoard, launchd, the extensions themselves) whose subsystem, category or
message mentions extensions, plug-ins, posters, widgets or an error.
One line per entry: time  process[pid]  subsystem:category  type  message
"""
import json, os, re, sys

PROCS = re.compile(r'lsd|pkd|runningboardd|extensionkit|PosterBoard|chronod|SpringBoard|'
                   r'launchd|Poster|Widget|appex|ExtensionFoundation|amfid|containermanagerd', re.I)
WORDS = re.compile(r'extension|plugin|plug-in|pkd|poster|widget|appex|ExtensionKit|EXExtension|'
                   r'NSExtension|launch|denied|deny|fail|error|invalid|entitle|sandbox|container|'
                   r'not found|unable|cannot|could not|refus|reject|abort|assert', re.I)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    kept = total = 0
    with open(sys.argv[1], errors='replace') as f:
        for line in f:
            total += 1
            try:
                e = json.loads(line)
            except ValueError:
                continue
            proc = os.path.basename(e.get('process') or '')
            if not PROCS.search(proc):
                continue
            sub = f"{e.get('subsystem', '')}:{e.get('category', '')}"
            msg = (e.get('message') or '').replace('\n', ' | ')
            ltype = str(e.get('log_type', ''))
            if not (WORDS.search(sub) or WORDS.search(msg) or ltype in ('Error', 'Fault')):
                continue
            kept += 1
            print(f"{e.get('timestamp', '')}  {proc}[{e.get('pid', '')}]  {sub}  {ltype}  {msg[:600]}")
    print(f"# kept {kept} of {total} entries", file=sys.stderr)


if __name__ == '__main__':
    main()
