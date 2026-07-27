# Notices and primary attribution

## Primary foundation: SysDVR

SwitchCast is a derivative of
[SysDVR](https://github.com/exelix11/SysDVR), created by **Exelix11** and
developed with the work of the **SysDVR contributors**.

SysDVR is the main base of SwitchCast. The project began by modifying the
SysDVR source tree, and it retains or adapts substantial portions of SysDVR's
Nintendo Switch capture implementation, sysmodule architecture, build and
process-management foundations, SysDVR-compatible USB protocol, Settings UI,
and dvr-patches integration.

SwitchCast's Cast transport, dedicated Pi Dock receiver, and standalone runtime
are additions to that foundation. A separate title ID does not change the
origin of the code.

The combined project is distributed under the GNU General Public License
version 2; see `LICENSE`. See `SYSDVR-ATTRIBUTION.md` for a fuller description
of the inherited foundation and the SwitchCast additions.

Upstream project: **https://github.com/exelix11/SysDVR**

Upstream base snapshot:
**https://github.com/exelix11/SysDVR/commit/804fd36e54983b7c76b249059b159f896af35b4d**

All vendored libraries, fonts, linked runtime libraries, and protocol
references are inventoried in `THIRD-PARTY-NOTICES.md`. The binary release
includes that file and the applicable standalone license texts.

## Cast protocol references

The Cast Streaming protocol implementation was independently written using the
public source of Chromium's
[Open Screen libcast](https://chromium.googlesource.com/openscreen/+/refs/heads/main/cast/),
which is distributed under a BSD-style license. No Open Screen source code is
copied into the SwitchCast binary.

[node-castv2-client](https://github.com/thibauts/node-castv2-client) by
Thibaut Séguy was also consulted for Cast V2 control-channel behavior. Its
JavaScript code is not vendored or linked into SwitchCast.

SwitchCast uses [libnx](https://github.com/switchbrew/libnx) APIs and the
devkitPro toolchain for Nintendo Switch homebrew.

The Settings application can download and manage
[dvr-patches](https://github.com/exelix11/dvr-patches), also by Exelix11 and
contributors. dvr-patches is a separate project and is not embedded in the
SwitchCast source or release archive.

Google Cast, Chromecast, Nintendo Switch, Atmosphère, SysDVR, Chromium, and
Open Screen are names of their respective owners. SwitchCast is an independent
experimental homebrew project and is not endorsed by those projects or
companies. SwitchCast changes and defects should not be attributed to the
upstream SysDVR developers.
