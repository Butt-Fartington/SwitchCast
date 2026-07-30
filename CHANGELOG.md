# Changelog

## 0.4.4 — 2026-07-30

- Extended the explicit zero-reorder, one-frame DPB SPS signaling to USB Dock
  mode.
- Rewrote the initial exact known GRC SPS in Annex-B packets before USB
  transmission.
- Changed periodically injected recovery SPS units to carry the low-delay VUI
  restrictions directly.
- Shared the exact-match SPS logic with Cast mode and added raw SPS, Annex-B,
  capacity, and fMP4 regression tests.
- Left unrecognized encoder configurations unchanged.

## 0.4.3 — 2026-07-29

- Added a Cast-only SPS substitution in the fMP4 `avcC` record.
- Explicitly signaled zero reordered frames and a one-frame decoded picture
  buffer for the known GRC 720p30 SPS.
- Preserved the original High Profile Level 3.2 encoding parameters, color
  description, one-reference-picture requirement, PPS, and captured slices.
- Left unrecognized SPS variants and the entire USB Dock path unchanged.

## 0.4.2 — 2026-07-27

- Replaced indefinite libnx USB writes with the SysDVR-derived
  timeout-and-cancel endpoint transport.
- Added a one-second deadline for every Dock transfer so physical cable loss
  cannot leave media or keepalive data pending across a replug.
- Restored the Switch panel and advanced the logical session immediately when
  an endpoint request times out.
- Preserved the validated H.264, PCM audio, and SwitchCast Dock 0.1.16
  pipelines.

## 0.4.1 — 2026-07-26

- Added independent USB keepalive and gameplay-freshness supervision.
- Restored the Switch panel within 1.5 seconds when USB gameplay video stops,
  even if the blocking capture call has not returned.
- Added SysDVR-compatible 48 kHz stereo PCM in USB Dock mode with a dedicated
  capture thread and serialized endpoint writes.
- Kept Cast mode video-only and kept inactive USB audio resources out of the
  Cast transport.
- Added session generations so a result captured for a dead USB session is
  never written into a replacement session.
- Paired with SwitchCast Dock 0.1.16 for HDMI audio, graphical status, idle
  video teardown/resume, and reconnect hardening.

## 0.4.0 — 2026-07-26

- Added a standalone, video-only USB Dock transport compatible with SysDVR
  protocol 03.
- Added a live Cast/USB selector and USB connection status to the Settings
  dashboard.
- Used a union for Cast buffers and the two statically allocated USB endpoint
  pages, with no general sysmodule heap.
- Avoided socket initialization when USB Dock is selected.
- Preserved guarded screen blanking across both transports.
- Added native USB protocol tests and SwitchCast Dock documentation.

## 0.3.2 — 2026-07-25

- Replaced three fixed media slots with a variable-size, up-to-24-frame
  retransmission history using the same temporary 2 MiB heap.
- Added controlled Cast media-session recovery for evicted NACK requests,
  bounded-NACK overflow, and stalled receiver checkpoint progress.
- Preserved failure and recovery evidence in
  `/config/switchcast/last-failure.json`.
- Added retained-history, feedback-age, checkpoint-age, eviction, high-water,
  and recovery telemetry to the JSON diagnostics.
- Kept the 90 ms Ultra-low and 150 ms Stable timing profiles unchanged.

## 0.3.1 — 2026-07-24

- Added an optional setting to blank the Switch backlight while gameplay video
  is actively streaming.
- Applied screen blanking live without restarting the Cast session.
- Kept the console display on whenever SwitchCast Settings is open, including
  when the Settings screen itself is present in the captured video.
- Restored the backlight when capture becomes inactive, a title exits, the
  receiver disconnects, or SwitchCast stops.
- Preserved an already-off backlight instead of unconditionally turning it on.

## 0.3.0 — 2026-07-24

- Added original SwitchCast branding and a dedicated Cast dashboard.
- Added ultra-low (90 ms requested playout) and stable (150 ms) profiles.
- Added receiver-reported latency and live transport status over IPC.
- Added automatic session restart when the latency profile changes.
- Tightened packet pacing and capture polling in ultra-low mode.
- Preserved the stable 0.2.1 transport behavior as a selectable fallback.
- Kept the standalone content ID, IPC service, and configuration directory.
- Kept audio disabled to prioritize video memory use and latency.
- Added a complete SysDVR lineage statement and third-party notice inventory.

## 0.2.1

- Improved title-transition recovery and capture rearming.
- Stabilized native Cast Streaming sessions across supported games.
- Reduced end-to-end latency compared with the HLS prototypes.

## 0.1.0

- First standalone SwitchCast prototype based on SysDVR.
