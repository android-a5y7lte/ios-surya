# Work waiting for the PC + phone

Things that can only be done at the PC (build, flash-boot, disassemble the dyld cache) or on the
phone (test). Everything else is prepared in the repo already. After any reboot, bring the link up:
`tools/sweet-usbnet.sh logs/testNN.txt` then `tools/surya-usb-internet.sh`.

## Ready to test now (kernels/tools already built or pushed)
- [ ] **GPU G2** (the main one). Kernel from run 41581107004 (xnu-kernel, patch 0100). Download, unpack,
      boot with `sweetgpu=2`, then `grep -a "SWEET GPU" logs/testNN.txt`. Watch for `cx_gdscr ... (PWR_ON)`,
      `gmu rcg ... (updated)`, branches `(on)`. If it hangs/resets, grab whatever SWEET GPU lines logged and
      Power+VolDown back to fastboot. (Hang => hypervisor blocks a CX write => rethink.)
- [ ] **suryablur** (white App Library / folders fix, no GPU). `scp tools/suryablur/suryablur
      root@192.168.7.2:/var/root/bin/`, then `ssh root@192.168.7.2 '/var/root/bin/suryablur 1'`, respring
      (kill SpringBoard). Photo before/after. `suryablur 0` reverts. Check if it helps Safari too.
- [ ] **surya-installapp** (sideload apps, no App Store). `tools/surya-installapp.py SOME.ipa` (or a .app),
      `--list`, `--remove NAME`. Try a small self-contained app first. Needs LaunchServices registration:
      if the icon does not appear, reboot once with sweetlsreset=1. Apps needing Apple online services won't run.
- [ ] **surya-crashlog** (why an app won't open). Open the failing app on the phone, wait a few seconds, then
      `tools/surya-crashlog.py AppName` (e.g. `MobileSafari`, `Maps`). Prints the exception, termination reason
      and faulting-thread symbols from the newest .ips. Run with no argument to list all recent crashes.
      Paste the output back here; the per-app fix follows from what it shows. Reads only.

## Camera (groundwork only - see SURYA_NOTES "Camera (surya)")
- [ ] **Camera C1** (read-only probe, patch 0101). Build the xnu kernel with 0101, boot with `sweetcam=1`,
      then `grep -a "SWEET CAM" logs/testNN.txt`. Expect camcc PLL0 / titan GDSC / CCI clock state; tells us
      whether the camera clocks + titan power are already up at boot. Zero-risk (reads only, same as GPU G1).
      Paste the SWEET CAM lines back. NOTE: camera is the hardest subsystem (ISP + an iOS capture provider) -
      C1/C2 only prove we can talk to a sensor; a live preview is far off. Details + staged plan in SURYA_NOTES.
- [ ] **Before C2**: get the IMX682 (+ other sensors') I2C slave address + sensor-id register from a surya
      vendor image (camxoverridesettings / sensor module libs) or datasheet. Needed to read a chip-id over CCI.

## Needs the PC's dyld cache (~/dsc) - disassembly, no phone needed
- [ ] **Audio next step**: extract the IOAudio2 "data exchange blocks" / "output streams" binary layout
      (commands in SURYA_NOTES.md "Audio next step"). Paste output back; then the stream code can be written.

## After G2 (depends on its result)
- [ ] **GPU G3**: GPU SMMU context (read-only dump first). Map ready in SURYA_NOTES.md "G3-G5 reference map".
- [ ] **GPU G4**: GMU firmware (a630_gmu.bin) + HFI. Needs a630_gmu.bin + a630_sqe.fw (linux-firmware) and
      the a615_zap we already have; decide ADT vs iOS-root delivery.

## Firmware to fetch from a stock surya vendor image (phone no longer runs Android)
- [ ] a630_gmu.bin, a630_sqe.fw (GPU) - linux-firmware qcom/
- [ ] QCA Bluetooth firmware crbtfw*.tlv + crnv*.bin (WCN3990) - from a surya vendor.img (not in our dump)
