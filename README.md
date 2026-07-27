# SwitchCast 0.4.0

![SwitchCast controller-and-video mark](work/assets/switchcast-controller-mark.png)

**Free and open-source software — GNU GPL version 2.**

SwitchCast sends the Nintendo Switch's captured H.264 gameplay video directly
to either a Chromecast/Google Cast TV over Wi-Fi or a SwitchCast Dock over
USB.

Cast mode uses the receiver's built-in Cast Streaming application (`0F5096E8`),
negotiates over Cast V2 TLS, and sends encrypted Cast RTP/RTCP over UDP. There
is no PC, phone, relay, hosted page, HLS server, custom Web Receiver, or Cast
Developer Console registration in the path.

USB Dock mode sends the same native capture through the SysDVR-compatible
protocol-03 bulk format to a Raspberry Pi Zero 2 W receiver. It bypasses Wi-Fi,
Cast negotiation, and receiver buffering for the lowest-latency path.

## Built on SysDVR

**SysDVR by Exelix11 and its contributors is the main foundation of
SwitchCast.** SwitchCast began as a direct modification of the SysDVR source
tree and inherits or adapts its Switch capture implementation, memory-conscious
sysmodule architecture, build and process-management foundations, Settings UI,
and dvr-patches integration.

SwitchCast adds the Google Cast transport, the dedicated Dock receiver, and a
standalone identity. “Standalone” means SysDVR does not have to be installed
at runtime; it does not diminish the project's lineage or the importance of
the upstream work.

Read `SYSDVR-ATTRIBUTION.md` for the detailed credit and visit the original
project at **https://github.com/exelix11/SysDVR**.

This repository is maintained as a fork of SysDVR so GitHub also preserves
the upstream commit history and displays the relationship on every repository
page. The SwitchCast work is based on SysDVR snapshot
[`804fd36`](https://github.com/exelix11/SysDVR/commit/804fd36e54983b7c76b249059b159f896af35b4d).

## New in 0.4.0

- Added a native, video-only **USB Dock** transport to the standalone
  SwitchCast sysmodule.
- Added a mutually exclusive **Cast over Wi-Fi / USB Dock** selector to the
  SwitchCast Settings dashboard.
- Retained SysDVR protocol-03 packet compatibility and prominent upstream
  attribution.
- Put the Cast working buffers and two USB endpoint pages in a union so the
  inactive transport consumes no parallel buffer allocation.
- Kept the socket subsystem offline in USB mode.
- Preserved screen blanking's safety rules in both transports: Settings stays
  visible, blanking begins only after gameplay video, and the panel restores
  on stop or disconnect.
- Added live USB initialization, dock-waiting, game-waiting, streaming, I/O,
  and capture status through IPC.
- Paired this release with SwitchCast Dock 0.1.15, whose HDMI link is 1080p60
  while the Switch's native continuous capture remains 1280x720 at about
  30 fps.

## New in 0.3.2

- The same temporary 2 MiB media heap now uses variable-size frame allocations
  and can retain up to 24 encoded frames instead of three fixed partitions.
- Late packet-loss feedback has a much deeper retransmission window without
  increasing the sysmodule memory budget or receiver delay.
- Unrecoverable NACKs and stalled receiver checkpoints refresh the native Cast
  media session automatically while the gameplay capture remains active.
- `/config/switchcast/last-failure.json` preserves the final recovery snapshot
  even after a successful automatic reconnection.
- Diagnostics now include retained history, arena high-water marks, evictions,
  feedback age, checkpoint age, recovery count, and the recovery reason.

## New in 0.3.1

- An optional **Blank Switch screen while streaming** setting turns off only
  the console backlight after the first gameplay frame is transmitted.
- The option applies live and restores the backlight when gameplay frames stop,
  a title exits, the receiver disconnects, or SwitchCast is stopped.
- Opening SwitchCast Settings always restores and keeps the console display on,
  even when the Settings screen itself is being captured.
- Screen blanking is off by default and never turns on a display that was
  already off before SwitchCast changed it.

## New in 0.3.0

- An original SwitchCast logo and indigo/cyan/coral interface.
- A dedicated dashboard with live enabled state, receiver, transport status,
  requested video delay, and receiver-reported playout delay.
- **Ultra-low** video mode requests a 90 ms receiver playout delay, uses
  tighter packet pacing, and polls the capture queue more frequently.
- **Stable** mode retains the proven 0.2.1 settings: a 150 ms delay request
  and more conservative packet pacing.
- Changing latency profiles while enabled automatically restarts the video
  session so the next Cast negotiation uses the new setting.
- Audio remains disabled. Game sound can continue playing locally without
  consuming SwitchCast's memory or Cast transport budget.

## Standalone SwitchCast

SwitchCast has its own:

- Atmosphère content ID: `00FF000053434153`
- IPC service: `swcast`
- configuration directory: `/config/switchcast`
- mutually exclusive Cast and USB Dock runtime and Settings UI

The installed binaries do not require, launch, replace, or communicate with a
separate SysDVR installation. The sysmodule compiles Switch capture, Cast and
USB Dock transports, and its small IPC controller; TCP, RTSP, audio, and the
legacy mode manager are not linked.

## What is included

- `SwitchCast.nro`: transport selector, receiver picker, enable/disable
  control, live status, boot preference, and dvr-patches manager.
- A dedicated Atmosphère sysmodule that remains alive while a game runs.
- Native OFFER/ANSWER negotiation for 1280x720 H.264 video.
- Direct SysDVR-compatible USB protocol-03 H.264 output for SwitchCast Dock.
- AES-128-CTR frame encryption using the Switch's hardware AES implementation.
- Cast RTP packetization, RTCP feedback, retransmission, and keyframe recovery.
- A variable-size, up-to-24-frame history in a 2 MiB game-only arena with no
  permanent media heap.
- Selectable 90 ms ultra-low and 150 ms stable video timing profiles.
- Automatic capture start when a compatible game launches.
- Clean session teardown and automatic rearming when a game or homebrew title
  exits, including receiver CLOSE messages during that transition.
- Video only. Audio remains deliberately disabled to protect memory and
  latency.

The foreground NRO is only the controller. The dedicated sysmodule is required
because an ordinary homebrew app cannot stay active beside a retail game.

## Install

1. Extract `SwitchCast-Standalone-v0.4.0.zip` to the SD-card root.
2. Reboot the console.
3. Open `/switch/SwitchCast.nro` from the Homebrew Menu.
4. Choose **Cast over Wi-Fi** and a receiver, or choose **USB Dock** and start
   the USB transport.
5. Optionally choose **Set current mode as default on boot**.
6. Exit the Settings app and launch a capture-compatible game.

The receiver list uses `_googlecast._tcp.local` multicast DNS. Automatic
selection and manual IPv4 entry are also available.

No `cast_app_id` file is used. There is no Cast Developer Console step.

## Build and contribute

See `building.md` for the devkitPro package versions and reproducible release
command. Contributions are welcome under the requirements in
`CONTRIBUTING.md`.

### Upgrading

Version 0.4.0 can be copied directly over 0.3.2, 0.3.1, 0.3.0, or 0.2.1. The
sysmodule and Settings application must both come from the same release
because the transport selector extends the SwitchCast IPC protocol.

### Upgrading from the 0.1.0 prototype

The prototype occupied the SysDVR content directory
`/atmosphere/contents/00FF0000A53BB665`. Version 0.4.0 does not overwrite or
remove that directory.

Before testing 0.4.0, make sure the old prototype or SysDVR is not also
streaming or enabled at boot. Two capture sysmodules can compete for memory and
the capture service. If `00FF0000A53BB665` contains the old SwitchCast
prototype, restore your SysDVR backup or remove that old prototype directory.

## Installed files

```text
/atmosphere/contents/00FF000053434153/exefs.nsp
/atmosphere/contents/00FF000053434153/flags/boot2.flag
/atmosphere/contents/00FF000053434153/toolbox.json
/switch/SwitchCast.nro
```

## Runtime files

```text
/config/switchcast/enabled          start SwitchCast automatically after boot
/config/switchcast/receiver_ip      selected receiver IPv4 address
/config/switchcast/latency_profile  `ultra` (90 ms) or `stable` (150 ms)
/config/switchcast/blank_screen     blank the console backlight during video
/config/switchcast/transport        `cast` or `usb`
/config/switchcast/debug.json       live transport and recovery statistics
/config/switchcast/last-failure.json preserved final stream failure/recovery
/config/switchcast/error.json       rejected Cast response, when available
```

## Expected status flow

```text
Armed; waiting for a capture-compatible game
  -> Discovering receiver
  -> Opening the Cast control channel
  -> Negotiating native Cast Streaming
  -> UDP video session ready; waiting for a keyframe
  -> Native low-latency gameplay video is streaming
```

When a game exits, the receiver, TLS/UDP sockets, queue, and temporary heap are
released. SwitchCast returns to **Armed; waiting for a capture-compatible
game**, ready for the next title without a reboot.

USB Dock mode follows the shorter path:

```text
USB device ready; waiting for SwitchCast Dock
  -> Dock connected; waiting for a capture-compatible game
  -> Native H.264 gameplay packets over USB 2.0
  -> Dock decoder and 1080p HDMI output
```

The source in that path remains the Switch capture service's fixed 720p
bitstream. Dock 0.1.15 scales it on the KMS display plane to a 1080p60 HDMI
mode without software resizing or re-encoding.

## Limitations

- Horizon's capture service supplies fixed 1280x720, approximately 30 fps
  H.264 and captures games, not the HOME menu.
- The Dock's 1080p HDMI mode improves output compatibility and chooses the
  Pi's display-plane scaler; it cannot restore detail absent from the 720p
  source.
- Games that opt out of capture require compatible dvr-patches. Those patches
  can introduce title-specific problems.
- Receiver firmware and TV processing differ. Some models may reject the
  built-in Cast Streaming application or choose a larger playout delay.
- The displayed target is SwitchCast's request. The receiver-reported delay
  and the TV's decode/display pipeline determine the actual glass-to-glass
  latency.
- Guest Wi-Fi, client isolation, VLAN filtering, or blocked multicast can
  prevent discovery or streaming.

See `CASTING.md` for the wire protocol and recovery design.

## Free and open source

SwitchCast is a derivative of SysDVR by Exelix11 and contributors and is
distributed under the GNU General Public License version 2. Anyone may use,
study, copy, modify, and redistribute it without paying a SwitchCast licensing
fee. If you distribute SwitchCast or a modified version, you must preserve the
GPL terms and make the corresponding source available as the license requires.
The GPL protects software freedom rather than imposing a noncommercial
restriction, so people may charge for copies or related services while every
recipient retains the same rights.

See [`LICENSE`](LICENSE), [`SYSDVR-ATTRIBUTION.md`](SYSDVR-ATTRIBUTION.md),
[`NOTICE.md`](NOTICE.md), and
[`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).

The source and binary release both carry the complete attribution bundle.
Vendored libraries, fonts, statically linked build dependencies, and
reference-only projects are inventoried separately so the SysDVR credit is
prominent without obscuring the other authors whose work is present.
