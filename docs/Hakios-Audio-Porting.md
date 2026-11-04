# HakiOS audio reference for POCO X3 NFC (surya)

Status: research notes only. This file does not add, enable, or copy an audio driver.

## Why this reference exists

HakiOS has demonstrated stereo loudspeaker playback on the POCO X3 NFC (surya, SM7150-AC / Snapdragon 732G). That is directly relevant hardware evidence for this project. The implementation must still be adapted to this repository's XNU revision, build system, device-tree representation, and driver interfaces; it is not a drop-in patch.

## Pinned upstream reference

- Repository: https://github.com/yavuzhanozyer/hakios-xnu-kernel
- Reference commit: [c3babcafb1943185fac678a779aaf6cd8a7a444c](https://github.com/yavuzhanozyer/hakios-xnu-kernel/commit/c3babcafb1943185fac678a779aaf6cd8a7a444c)
- Relevant stereo milestone: [04ffea08af12e76b09509d08ea82cba5b2162263](https://github.com/yavuzhanozyer/hakios-xnu-kernel/commit/04ffea08af12e76b09509d08ea82cba5b2162263)
- Device status: [devices/surya/device.conf at the reference commit](https://github.com/yavuzhanozyer/hakios-xnu-kernel/blob/c3babcafb1943185fac678a779aaf6cd8a7a444c/devices/surya/device.conf)
- Device engineering notes: [devices/surya/README.md](https://github.com/yavuzhanozyer/hakios-xnu-kernel/blob/c3babcafb1943185fac678a779aaf6cd8a7a444c/devices/surya/README.md)
- Changelog: [CHANGELOG.md](https://github.com/yavuzhanozyer/hakios-xnu-kernel/blob/c3babcafb1943185fac678a779aaf6cd8a7a444c/CHANGELOG.md)

Use the pinned commit for repeatable comparisons. Do not silently replace it with a moving branch head; record any newer upstream revision separately.

## What HakiOS has actually demonstrated

The upstream device status, dated 3 November 2026, reports both built-in speakers working in stereo with audio played by iOS. It identifies:

- Qualcomm audio DSP: ADSP firmware started and connected; the device notes describe TrustZone/PIL startup, GLINK, QRTR and APR audio services.
- Audio transport: TERTIARY MI2S through the DSP, AFE port 0x1004, with GPIO 53–56 for clock, frame sync and data.
- DMA/IOMMU: the sample stream uses SMMU stream ID 0x1b21 on this device.
- Amplifiers: two TI TAS256x-family devices on I²C controller `i2c@a8c000`: right at 0x4c (reset GPIO 91) and left at 0x4d (reset through PM6150L GPIO 8).
- Format: Android's corresponding speaker route is documented as two-channel, S24_LE, 48 kHz. This is useful hardware reference, not proof that the XNU driver must expose the same software format.
- Safe level: the stereo milestone sets the default 12 dB below the amplifiers' 0 dB setting. The upstream notes report crackling when driven too close to maximum without the Android DSP speaker-protection path.

### Explicit limitations

- HakiOS reports **no microphone support** for surya.
- Its status explicitly says speaker protection provided by Android's DSP is not present in this implementation.
- Do not assume earpiece, headset output, headset detection, or microphone capture works; the cited status does not establish those paths.
- The right and left amplifier reset/control paths differ. Treating them as identical is unsafe.

## Porting plan for ios-surya

Work in small, independently testable stages. Do not import the whole HakiOS tree or its device configuration wholesale.

1. **Baseline inventory**
   - Record this repository's current XNU tag/commit, patch series, target build configuration, ADT/DT inputs, and existing audio-related stubs.
   - Confirm the exact Surya board variant and Huaxing display configuration remain unchanged.
   - Keep firmware, calibration data, and device-specific dumps out of Git.

2. **Hardware description**
   - Verify the live device tree and the matching Android kernel/device-tree sources for the ADSP, GLINK/QRTR/APR, SMMU stream ID, MI2S pins, I²C controller, both TAS256x addresses, reset GPIOs, and interrupt GPIOs.
   - Cross-check against the phone itself. Do not write registers based only on a device-tree guess.

3. **DSP transport**
   - Understand the existing XNU platform/interrupt/memory/DMA abstractions first.
   - Port only the required ADSP firmware loading and TrustZone/GLINK/QRTR/APR sequence, with explicit timeouts and diagnostic logging.
   - Verify DSP readiness and service discovery before attempting playback.

4. **MI2S and amplifier bring-up**
   - Establish the TERTIARY MI2S clocking and pin mux with playback muted.
   - Probe and reset the two amplifiers independently; verify each I²C response and identity/register state.
   - Configure sample format, channel mapping and rate only after the hardware path is stable.
   - Start at a conservative output level. Do not default to amplifier maximum; implement a conservative cap until speaker protection and thermal/current limits are understood.

5. **Playback validation**
   - Test right channel alone, left channel alone, then stereo.
   - Use a low-level test tone and a short playback buffer before integrating the iOS audio stack.
   - Record kernel logs, measured output behavior, resets, underruns and thermal behavior for each test.

6. **Capture and other routes**
   - Treat WCD9375/SoundWire microphone, earpiece and headset paths as separate follow-up work. They are not prerequisites for proving the loudspeaker path and are not shown as working by the cited HakiOS status.

## Compatibility and licensing guardrails

- HakiOS is based on XNU xnu-8796.121.2 according to its source documentation. Confirm this repository's exact XNU base and local patches before adapting any implementation.
- HakiOS's driver/configuration model is project-specific. Its `CONFIG_HAKIOS_*` options, device repository layout, boot arguments, and helper scripts are not automatically compatible with this project.
- Use upstream code as a technical reference first. Before copying or adapting source, inspect the exact file, copyright header, and license; preserve required notices and meet the applicable license conditions.
- Do not copy phone firmware, calibration blobs, Apple userland files, IPSW contents, keys, or personal device backups into this repository.
- Do not overwrite current audio work. Port into a clearly isolated implementation and integrate only after review and build checks.

## Repository-state warning at the time this note was created

The remote `surya` branch of `android-a5y7lte/ios-surya` currently exposes a README and NOTES that still describe the Redmi Note 10 Pro / `sweet` project, despite this repository's Surya description. This may simply mean the active Surya work is local or on another branch and has not been pushed to that remote branch.

**Before using this branch as a code-integration base, reconcile it with the branch and uncommitted working tree Claude Code is currently using.** This reference branch was created from the remote `surya` branch; it cannot contain Claude Code changes that exist only in a local, unpushed working tree. Keep those local changes safe and do not reset, checkout over, or force-push the active Claude Code branch.

## Immediate next action

Ask Claude Code to inspect this document and compare the pinned HakiOS reference against the actual current Surya working tree. Have it produce an inventory and compatibility report first; do not ask it to copy drivers or change the build until that report is reviewed.
