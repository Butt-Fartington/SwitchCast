# SwitchCast native streaming design

SwitchCast 0.4.0 has two mutually exclusive video transports. The Cast design
below remains unchanged. USB Dock mode bypasses the Cast session entirely and
uses the separate path documented after it.

## Session flow

```text
Switch game capture (H.264 Annex B, 1280x720 @ ~30 fps)
  -> Cast V2 TLS at receiver port 8009
  -> launch built-in Cast Streaming A/V app 0F5096E8
  -> connect to the returned transport ID
  -> OFFER on urn:x-cast:com.google.cast.webrtc
  <- ANSWER containing UDP port, selected index, and receiver SSRC
  -> AES-128-CTR encrypted Cast RTP/H.264 over UDP
  <- compound RTCP checkpoints, NACKs, and PLI
```

This deliberately avoids the Web Receiver/media-player route. There is no
HTTP server, JavaScript receiver, MSE buffer, fragmented MP4, HLS playlist, or
developer application ID in the path.

## OFFER

The sender offers one `video_source` stream:

- Cast mode: `mirroring`
- codec: H.264
- codec parameter: derived from the captured SPS (`avc1.xxxxxx`)
- RTP profile: `cast`
- payload type: 101
- time base: 90,000
- resolution: 1280x720
- maximum frame rate: 30
- maximum bitrate: 8 Mbps
- target playout delay: 90 ms in Ultra-low mode (default), or 150 ms in
  Stable mode
- per-session random sender SSRC, AES key, and IV mask

The answer must select stream index zero and provide a nonzero receiver SSRC
and UDP port. Rejected or malformed answers are preserved in
`/config/switchcast/error.json`.

## Frame encryption

Every complete Annex-B access unit is encrypted independently with AES-128 in
CTR mode. The 16-byte counter begins as zero, the big-endian 32-bit frame ID is
written at bytes 8-11, and all 16 bytes are XORed with the session IV mask.

The sysmodule uses libnx's hardware-accelerated `aes128CtrCrypt`. Session
secrets come from Horizon's `csrng` service; a tick-seeded xorshift fallback is
kept only so a service failure produces a diagnosable session instead of a
boot crash.

## RTP

Each IPv4 UDP datagram is bounded below the Ethernet MTU:

- 1472-byte maximum RTP datagram
- 19-byte normal RTP + Cast header
- 23 bytes reserved when calculating chunk size for optional extensions
- 1449-byte maximum encrypted frame chunk
- monotonically increasing random-start RTP sequence number
- 8-bit truncated frame/reference IDs in the Cast header
- keyframes reference themselves; dependent frames reference the prior frame

Ultra-low mode paces frames in groups of twelve packets with a 100 microsecond
yield and polls an empty send queue every 250 microseconds. Stable mode retains
groups of eight packets, a 250 microsecond yield, and a 1 millisecond empty
queue poll. Both modes reduce large keyframe bursts without adding a
frame-sized software queue.

## RTCP and repair

SwitchCast sends a 28-byte RTCP Sender Report before media and every 500 ms.
It consumes compound receiver packets and handles:

- CAST checkpoint and effective playout-delay feedback
- specific packet NACKs
- all-packets-lost NACKs
- Picture Loss Indicator (PLI)

Acknowledged history is released immediately. Up to 24 descriptors share a
variable-size 2 MiB arena, allowing many normal encoded frames to remain
available for retransmission while still admitting large keyframes. This
replaces the old three-way fixed partition without increasing the heap.

If requested history has already been evicted, a feedback packet exceeds the
bounded NACK list, or receiver checkpoints stop advancing for three seconds
while frames remain outstanding, SwitchCast refreshes the Cast media session.
Capture remains active, the receiver gets new session keys and transport state,
and transmission resumes at the next IDR instead of leaving a stalled decoder
chain in place.

## Game lifecycle and memory

The sysmodule stays armed without allocating the 2 MiB media heap. When a game
process runs, capture is enabled and SPS/PPS are observed. Cast launch begins
only after valid H.264 setup data arrives.

At game exit, capture, UDP, TLS, receiver session, queue state, and the
temporary heap are released. The sysmodule returns to the armed state for the
next game.

When screen blanking is enabled, SwitchCast turns off only the console
backlight after the first gameplay frame is transmitted. It restores the
backlight when gameplay frames have been absent for three seconds, the title
exits, the session disconnects, or SwitchCast stops. It records whether it
actually changed the backlight so cleanup does not turn on a display that was
already off. While SwitchCast Settings holds its IPC session open, the
sysmodule restores and keeps the console display on even if the Settings screen
is part of the captured stream.

Receiver CLOSE or control-channel loss during the same transition is classified
as normal gameplay shutdown. It no longer parks the sender in a permanent
failure state. Codec, launch, and transport failures also retry with bounded
delays while gameplay remains active.

## USB Dock transport

USB Dock mode retains the SysDVR protocol-03 wire format so the standalone
SwitchCast sysmodule can feed SwitchCast Dock without a second capture
sysmodule:

```text
Switch grc:d capture (Annex-B H.264, 1280x720 @ ~30 fps)
  -> protocol-03 hello and video-only handshake
  -> one packed SysDVR PacketHeader + access unit per USB bulk write
  -> Pi Zero 2 W parser/decoder
  -> DRM/KMS display plane
  -> 1920x1080p60 HDMI link
```

The transport requests and reinjects SPS/PPS on every IDR, disables SysDVR's
NAL replay optimization on the reliable USB link, and opens no network
sockets. Cast working memory and the two 4 KiB USB endpoint pages share a
union, so the USB pages add no reservation on top of the larger Cast working
buffer set.

The Switch-side source resolution is fixed by Horizon's continuous game
capture service. The Dock scales the decoded 1280x720 frame on the KMS display
plane; it does not re-encode or claim a 1080p capture.

Screen blanking uses the same guarded policy as Cast mode. It begins only after
a gameplay packet is delivered, stays disabled while SwitchCast Settings is
visible, and restores the panel on USB stop or disconnect.

## Diagnostics

`debug.json` records the selected receiver, negotiated UDP port and SSRCs,
latency profile, requested and receiver-reported delay, frames
queued/sent/dropped, RTP/RTCP counts, retransmissions, NACKs, PLI,
unrecoverable requests, retained-history usage and high-water marks, feedback
and checkpoint ages, evictions, recovery count, and the recovery reason. It is
updated at negotiation, stream start, every five seconds, and shutdown/failure.

`last-failure.json` is written before a failed or automatically refreshed media
session is torn down. Unlike the live diagnostic, it is not cleared when a new
game starts or overwritten by a successful reconnection.

## Reference basis

The implementation was checked against Open Screen revision
`b13215d275c0c1661cf3d7c19f55ad7f59020938` (2026-07-16), specifically:

- `cast/common/public/cast_streaming_app_ids.h`
- `cast/streaming/message_fields.h`
- `cast/streaming/public/offer_messages.cc`
- `cast/streaming/public/answer_messages.cc`
- `cast/streaming/impl/frame_crypto.cc`
- `cast/streaming/impl/rtp_packetizer.cc`
- `cast/streaming/impl/rtp_defines.h`
- `cast/streaming/impl/sender_report_builder.cc`
- `cast/streaming/impl/compound_rtcp_parser.cc`

Primary upstream:

- https://chromium.googlesource.com/openscreen/+/refs/heads/main/cast/
- https://chromium.googlesource.com/chromium/src/+/HEAD/media/cast/

The Cast V2 control-channel behavior was also checked against
https://github.com/thibauts/node-castv2-client (MIT). No JavaScript source from
that project is vendored or linked into SwitchCast.

Chromium explicitly configures Cast H.264 encoders for Annex-B output because
it is better supported by Cast receivers. Horizon's captured gameplay stream
already provides Annex-B access units, so SwitchCast does not repackage the
bitstream.

Open Screen defines target playout delay as the capture-to-presentation
window, accepts offer values from 0 through 5000 ms, and uses roughly two
30-fps frames (66 ms) as the sender's low-latency in-flight floor. SwitchCast's
90 ms Ultra-low request stays above that floor while substantially undercutting
Open Screen's 400 ms default. The 150 ms profile remains available because
receiver firmware and Wi-Fi loss recovery vary.
