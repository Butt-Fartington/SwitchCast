# Changelog

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
