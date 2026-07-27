# SwitchCast is built on SysDVR

SwitchCast would not exist without
[SysDVR](https://github.com/exelix11/SysDVR), created by **Exelix11** and
developed with the work of the **SysDVR contributors**.

SwitchCast began as a direct modification of the SysDVR source tree. SysDVR is
the principal technical foundation of this project—not a minor dependency or
an incidental reference.

The public SwitchCast repository is a GitHub fork of SysDVR and retains the
upstream commit graph. The SwitchCast source snapshot was based on SysDVR
commit
[`804fd36e54983b7c76b249059b159f896af35b4d`](https://github.com/exelix11/SysDVR/commit/804fd36e54983b7c76b249059b159f896af35b4d).
That relationship is also recorded by the `upstream` Git remote.

Substantial foundations inherited or adapted from SysDVR include:

- Nintendo Switch gameplay capture through Horizon's `grc:d` services.
- Capture initialization, H.264 packet handling, SPS/PPS reinjection, and
  capture-compatibility behavior.
- USB device identity, protocol-03 handshake, packed video framing, and the
  direct USB transport design used by SwitchCast Dock mode.
- The timeout-and-cancel USB endpoint implementation adapted in SwitchCast
  0.4.2 from SysDVR's `UsbComms.c` and `Serial.c`, which provides the hard
  disconnect boundary needed for reliable physical unplug/replug recovery.
- The memory-conscious Atmosphère sysmodule architecture and static-buffer
  approach.
- The original build, packaging, process-management, and IPC foundations.
- The Settings NRO/UI foundation and dvr-patches integration.
- The translation catalog and receiver-independent settings infrastructure.
- Years of practical investigation into Switch capture behavior and
  game-specific compatibility.

SwitchCast adds and specializes the Google Cast side—receiver discovery, Cast
V2 control, native Cast Streaming OFFER/ANSWER negotiation, encrypted RTP/RTCP
video delivery, retransmission, latency tuning, status reporting, and automatic
session recovery—and adds the dedicated Raspberry Pi SwitchCast Dock receiver.

The standalone SwitchCast title ID and service mean SysDVR does not need to be
installed at runtime. They do **not** erase the project's lineage. SwitchCast
is a derivative work of SysDVR and remains distributed under the **GNU General
Public License version 2**.

Legacy SysDVR PC clients and TCP/RTSP transports are not part of the
standalone SwitchCast build or public source set. SwitchCast's dedicated USB
Dock mode uses the same protocol lineage, SysDVR-derived endpoint transport,
and PCM format in a purpose-built receiver path. Excluding other unused
subsystems keeps the published tree aligned with the binaries; it does not
change the credit for the substantial SysDVR code that remains.

Third-party libraries and data that arrived through the SysDVR source tree are
credited individually in `THIRD-PARTY-NOTICES.md`. Their original file-level
license notices have also been preserved.

Please visit and support the original project:

**https://github.com/exelix11/SysDVR**

The SwitchCast project is independent and its changes or defects should not be
attributed to Exelix11 or the upstream SysDVR contributors.
