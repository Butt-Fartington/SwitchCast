# SwitchCast 0.3.0

SwitchCast sends captured Nintendo Switch gameplay video directly to a
Chromecast or Google Cast TV on the local network. It uses the receiver's
built-in Cast Streaming application: no PC, phone, relay, hosted receiver, or
Cast Developer Console registration is required.

## Highlights

- Original SwitchCast interface and branding.
- Ultra-low and stable latency profiles.
- Receiver-reported playout delay and live transport status.
- Automatic restart when the selected latency profile changes.
- Standalone Atmosphère content ID, IPC service, and configuration directory.
- Video-only design tested in extended gameplay sessions.

## Install

Extract `SwitchCast-Standalone-v0.3.0.zip` to the SD-card root, reboot, open
`/switch/SwitchCast.nro`, select a receiver, then launch a
capture-compatible game.

Do not run SysDVR or another capture sysmodule at the same time.

## Attribution

SwitchCast is built on SysDVR by Exelix11 and contributors. SysDVR is the
principal technical foundation of this project. The release archive includes
the GPL-2.0 license, detailed SysDVR lineage, and complete third-party notices.
