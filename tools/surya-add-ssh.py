#!/usr/bin/env python3
"""Add an OpenSSH server (and bash/coreutils) to an iOS root image for surya.

  sudo tools/surya-add-ssh.py MOUNTED_ROOT DEBDIR PUBKEY

MOUNTED_ROOT  the HFS+ root image mounted read-write (as for
              tools/sweet-root-tweaks.py: `fsck.hfsplus -fy IMG`, then
              `mount -t hfsplus -o loop,force IMG MNT`)
DEBDIR        the Elucubratus .debs (openssh-server, openssh-client,
              libssl1.1.1, bash, coreutils, coreutils-bin, grep, sed, ncurses,
              ncurses5-libs, readline, pcre2, profile.d, ca-certificates)
PUBKEY        your SSH public key (e.g. ~/.ssh/id_ed25519.pub) -> root's
              authorized_keys (key login only, no passwords)

What it changes in the image (run as root, idempotent):
  - unpacks every .deb's data part into the root (tar --keep-directory-symlink,
    so /etc, /var, /tmp stay symlinks into /private)
  - /bin/sh -> bash if the image has no /bin/sh
  - host keys /etc/ssh/ssh_host_{ed25519,rsa}_key (generated here, once)
  - etc/master.passwd, passwd, group: 'sshd' alias of iOS's _sshd (uid 75),
    OpenSSH's privilege-separation user
  - /etc/ssh/sshd_config: appends a surya block (key login for root,
    no passwords, no PAM, no DNS lookups)
  - /var/root/.ssh/authorized_keys (0700/0600, owner root)
  - /System/Library/xpc/launchd.plist: job com.openssh.sshd
    (/usr/sbin/sshd -D -e, RunAtLoad, KeepAlive, stderr to /dev/console so
    sshd's errors land in the kernel log / USB live log)
The phone then listens on 192.168.7.2:22 (kernel USB Ethernet, sweetusb=2):
  ssh root@192.168.7.2
"""
import glob, os, plistlib, shutil, subprocess, sys

SSHD_BLOCK = """
# --- surya (tools/surya-add-ssh.py) ---
PermitRootLogin prohibit-password
PubkeyAuthentication yes
PasswordAuthentication no
KbdInteractiveAuthentication no
UsePAM no
UseDNS no
AuthorizedKeysFile .ssh/authorized_keys
"""


def run(*cmd):
    print('+', ' '.join(cmd))
    subprocess.run(cmd, check=True)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    root, debdir, pubkey = sys.argv[1:]
    if os.geteuid() != 0:
        sys.exit('run as root (sudo): files must be owned by root/mobile')
    if not os.path.isdir(os.path.join(root, 'System/Library/xpc')):
        sys.exit(f'{root}: not a mounted iOS root (no System/Library/xpc)')
    debs = sorted(glob.glob(os.path.join(debdir, '*.deb')))
    if not debs:
        sys.exit(f'{debdir}: no .deb files')

    # iOS keeps /etc, /var, /tmp as symlinks into /private. `dpkg-deb -x`
    # (plain tar) would replace those symlinks with real directories, so
    # extract with --keep-directory-symlink, and refuse to go on if a
    # symlink is already broken (e.g. by an earlier dpkg-deb -x run).
    for link in ('etc', 'var', 'tmp'):
        if not os.path.islink(os.path.join(root, link)):
            sys.exit(f'{root}/{link} is not a symlink: start again from a fresh copy of the image')
    for deb in debs:
        print('+ extract', os.path.basename(deb))
        fs = subprocess.Popen(['dpkg-deb', '--fsys-tarfile', deb], stdout=subprocess.PIPE)
        subprocess.run(['tar', '-x', '--keep-directory-symlink', '-C', root], stdin=fs.stdout, check=True)
        if fs.wait() != 0:
            sys.exit(f'dpkg-deb failed on {deb}')
    for link in ('etc', 'var', 'tmp'):
        if not os.path.islink(os.path.join(root, link)):
            sys.exit(f'{root}/{link} was replaced by a directory: image copy is broken')

    sh = os.path.join(root, 'bin/sh')
    if not os.path.lexists(sh) and os.path.exists(os.path.join(root, 'bin/bash')):
        os.symlink('bash', sh)
        print('bin/sh -> bash')

    etc = os.path.join(root, 'private/etc') if os.path.isdir(os.path.join(root, 'private/etc')) else os.path.join(root, 'etc')
    sshdir = os.path.join(etc, 'ssh')
    os.makedirs(sshdir, exist_ok=True)
    for kind in ('ed25519', 'rsa'):
        key = os.path.join(sshdir, f'ssh_host_{kind}_key')
        if not os.path.exists(key):
            run('ssh-keygen', '-q', '-t', kind, '-N', '', '-C', 'surya', '-f', key)
    cfg = os.path.join(sshdir, 'sshd_config')
    text = open(cfg).read() if os.path.exists(cfg) else ''
    if '# --- surya' not in text:
        with open(cfg, 'a') as f:
            f.write(SSHD_BLOCK)
        print(f'{cfg}: surya block appended')

    # OpenSSH's privilege-separation user is "sshd" (strings in usr/sbin/sshd);
    # iOS ships it as "_sshd" (uid/gid 75). Add an "sshd" alias with the same ids.
    for name, line in (('master.passwd', 'sshd:*:75:75::0:0:sshd Privilege separation:/var/empty:/usr/bin/false\n'),
                       ('passwd', 'sshd:*:75:75:sshd Privilege separation:/var/empty:/usr/bin/false\n'),
                       ('group', 'sshd:*:75:\n')):
        path = os.path.join(etc, name)
        if os.path.exists(path):
            text = open(path).read()
            if not any(l.startswith('sshd:') for l in text.splitlines()):
                with open(path, 'a') as f:
                    if text and not text.endswith('\n'):
                        f.write('\n')
                    f.write(line)
                print(f'{path}: sshd alias added')

    var = os.path.join(root, 'private/var') if os.path.isdir(os.path.join(root, 'private/var')) else os.path.join(root, 'var')
    dotssh = os.path.join(var, 'root/.ssh')
    os.makedirs(dotssh, exist_ok=True)
    os.chmod(dotssh, 0o700)
    ak = os.path.join(dotssh, 'authorized_keys')
    shutil.copyfile(pubkey, ak)
    os.chmod(ak, 0o600)
    os.chown(dotssh, 0, 0)
    os.chown(ak, 0, 0)
    os.makedirs(os.path.join(var, 'empty'), exist_ok=True)   # privsep chroot dir
    print(f'{ak}: installed')

    lp = os.path.join(root, 'System/Library/xpc/launchd.plist')
    with open(lp, 'rb') as f:
        plist = plistlib.load(f)
    plist['LaunchDaemons']['/System/Library/LaunchDaemons/com.openssh.sshd.plist'] = {
        'Label': 'com.openssh.sshd',
        'ProgramArguments': ['/usr/sbin/sshd', '-D', '-e'],
        'RunAtLoad': True,
        'KeepAlive': True,
        'UserName': 'root',
        'StandardErrorPath': '/dev/console',
        'StandardOutPath': '/dev/console',
    }
    with open(lp, 'wb') as f:
        plistlib.dump(plist, f, fmt=plistlib.FMT_BINARY)
    print(f'{lp}: job com.openssh.sshd added')
    print('done: unmount, then make the sparse image and flash (see SURYA_NOTES.md)')


if __name__ == '__main__':
    main()
