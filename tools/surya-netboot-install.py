#!/usr/bin/env python3
"""Install the boot-time internet job on the phone (over SSH, phone running).

  tools/surya-netboot-install.py            install
  tools/surya-netboot-install.py --remove   take the job out again

Copies tools/surya-netboot.sh and tools/suryanet/suryanet to /var/root/bin, then adds
the launchd job com.surya.netboot (RunAtLoad, /bin/bash /var/root/bin/surya-netboot.sh)
to /System/Library/xpc/launchd.plist, as tools/surya-add-ssh.py did for sshd.
The phone keeps a copy of the file it had before: launchd.plist.before-netboot.
The edited plist is read back on the PC before it is uploaded. Takes effect at the
next boot. The PC half (forwarding/NAT) is still tools/surya-usb-internet.sh.
"""
import os, plistlib, subprocess, sys, tempfile

PHONE = 'root@192.168.7.2'
LP = '/System/Library/xpc/launchd.plist'
KEY = '/System/Library/LaunchDaemons/com.surya.netboot.plist'
HERE = os.path.dirname(os.path.abspath(__file__))


def run(*cmd):
    print('+', ' '.join(cmd))
    subprocess.run(cmd, check=True)


def main():
    remove = '--remove' in sys.argv[1:]
    if not remove:
        run('ssh', PHONE, 'mkdir -p /var/root/bin')
        run('scp', '-q', os.path.join(HERE, 'surya-netboot.sh'), os.path.join(HERE, 'suryanet', 'suryanet'),
            PHONE + ':/var/root/bin/')
        run('ssh', PHONE, 'chmod 755 /var/root/bin/surya-netboot.sh /var/root/bin/suryanet; '
            'test -x /var/root/bin/route || echo "WARNING: /var/root/bin/route missing (see SURYA_NOTES.md)"')
    with tempfile.TemporaryDirectory() as d:
        local = os.path.join(d, 'launchd.plist')
        run('scp', '-q', PHONE + ':' + LP, local)
        with open(local, 'rb') as f:
            plist = plistlib.load(f)
        jobs = plist['LaunchDaemons']
        if remove:
            if KEY not in jobs:
                print('job not installed; nothing to do')
                return
            del jobs[KEY]
        else:
            jobs[KEY] = {
                'Label': 'com.surya.netboot',
                'ProgramArguments': ['/bin/bash', '/var/root/bin/surya-netboot.sh'],
                'RunAtLoad': True,
                'KeepAlive': False,
                'UserName': 'root',
            }
        with open(local, 'wb') as f:
            plistlib.dump(plist, f, fmt=plistlib.FMT_BINARY)
        with open(local, 'rb') as f:               # read back before it goes to the phone
            check = plistlib.load(f)
        assert (KEY in check['LaunchDaemons']) != remove
        assert len(check['LaunchDaemons']) == len(jobs)
        run('ssh', PHONE, f'test -e {LP}.before-netboot || cp -p {LP} {LP}.before-netboot')
        run('scp', '-q', local, PHONE + ':' + LP + '.new')
        run('ssh', PHONE, f'mv {LP}.new {LP} && ls -la {LP} {LP}.before-netboot')
    print('removed; takes effect at the next boot' if remove else
          'installed; at the next boot see /var/root/netboot.log on the phone')


if __name__ == '__main__':
    main()
