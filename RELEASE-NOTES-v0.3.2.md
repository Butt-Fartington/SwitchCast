# SwitchCast 0.3.2

SwitchCast 0.3.2 strengthens long-session packet-loss recovery without
increasing the sysmodule memory budget or changing the proven latency profiles.

## What changed

- Replaced three fixed ~683 KiB frame slots with a variable-size 2 MiB arena
  and up to 24 retained frame descriptors.
- Normal gameplay now keeps substantially deeper retransmission history while
  preserving room for large H.264 keyframes.
- An evicted-frame NACK, an oversized NACK burst, or three seconds without
  receiver checkpoint progress now triggers a controlled Cast media-session
  refresh.
- Gameplay capture remains active during recovery and resumes at a clean IDR.
- Added `/config/switchcast/last-failure.json`, which survives successful
  reconnection and later game launches.
- Expanded diagnostics with current/peak retained history, evictions,
  feedback and checkpoint ages, recovery count, numeric status, and the
  recovery reason.
- Ultra-low remains 90 ms and Stable remains 150 ms.

## Install

Extract `SwitchCast-Standalone-v0.3.2.zip` to the SD-card root and reboot.
Existing receiver, latency, boot, and screen-blanking settings are preserved.

Do not run SysDVR or another capture sysmodule at the same time.

## Attribution and license

SwitchCast is free and open-source software under GPL-2.0 and is built on
SysDVR by Exelix11 and contributors. SysDVR remains the principal technical
foundation. The release contains the full license, detailed lineage, source
links, and third-party notices.
