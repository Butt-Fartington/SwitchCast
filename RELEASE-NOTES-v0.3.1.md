# SwitchCast 0.3.1

SwitchCast 0.3.1 adds optional, fail-safe console screen blanking while keeping
the proven 0.3.0 Cast video path unchanged.

## What changed

- Added **Blank Switch screen while streaming** to SwitchCast Settings.
- The setting is off by default and persists across reboots.
- The setting applies live without restarting the Cast receiver.
- SwitchCast blanks only the console backlight and continues capturing gameplay.
- Opening SwitchCast Settings always restores and keeps the console screen on,
  including when the Settings screen is visible in the cast.
- The backlight is restored when gameplay frames stop, a title exits, the
  receiver disconnects, or SwitchCast is stopped.
- If the display was already off, SwitchCast does not turn it on during cleanup.

## Install

Extract `SwitchCast-Standalone-v0.3.1.zip` to the SD-card root and reboot. Open
`/switch/SwitchCast.nro` to turn screen blanking on or off.

Do not run SysDVR or another capture sysmodule at the same time.

## Attribution and license

SwitchCast is free and open-source software under GPL-2.0 and is built on
SysDVR by Exelix11 and contributors. SysDVR remains the principal technical
foundation. The release contains the full license, detailed lineage, source
links, and third-party notices.
