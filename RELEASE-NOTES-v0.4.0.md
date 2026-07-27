# SwitchCast 0.4.0

SwitchCast 0.4.0 adds the native USB half of the project without giving up the
working Chromecast path.

## Highlights

- Choose **Cast over Wi-Fi** or **USB Dock** from the main SwitchCast Settings
  screen.
- USB Dock mode is video-only, compatible with SysDVR protocol 03, and runs
  directly from the standalone SwitchCast sysmodule.
- A separate SysDVR installation is no longer needed to feed SwitchCast Dock.
- Cast and USB buffers are mutually exclusive, and USB mode does not initialize
  the network stack.
- Live USB states distinguish device initialization, waiting for the Dock,
  waiting for gameplay, streaming, I/O failure, and capture failure.
- Optional console screen blanking keeps the same safety behavior in either
  transport.

## Matched Dock image

Use SwitchCast Dock 0.1.15 with this release. It starts directly on the
physically validated FFmpeg decode path, avoiding the failed VideoCore-first
attempt that could consume the first clean keyframe. The HDMI link is fixed at
1920x1080p60 and the KMS plane scales the decoded frame.

The Nintendo Switch continuous capture source is still fixed at 1280x720 and
approximately 30 fps. The Dock's 1080p output mode changes the HDMI/scaling
path; it does not create 1080p source detail or add another encode.

## Installation

Extract `SwitchCast-Standalone-v0.4.0.zip` to the SD-card root and reboot. The
sysmodule and Settings NRO must come from this same release because 0.4.0 uses
IPC protocol version 4.

Do not enable upstream SysDVR at the same time as SwitchCast. Both would
compete for the same capture service and USB identity.

## Attribution

SwitchCast is built on SysDVR by Exelix11 and contributors. The capture system,
memory-conscious sysmodule foundation, protocol-03 USB format, Settings and
dvr-patches foundations, and other inherited work remain prominently credited
and licensed under GNU GPL version 2.
