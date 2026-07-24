# SwitchCast native streaming design

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

Acknowledged history is released immediately. Up to three encoded frames share
the temporary 2 MiB heap and may be retained for retransmission. If requested
history is already gone, SwitchCast waits for an IDR so it does not send a
chain of undecodable predicted frames.

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

## Diagnostics

`debug.json` records the selected receiver, negotiated UDP port and SSRCs,
latency profile, requested and receiver-reported delay, frames
queued/sent/dropped, RTP/RTCP counts, retransmissions, NACKs, PLI, and
unrecoverable requests. It is updated at negotiation, stream start, every five
seconds, and shutdown/failure.

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
