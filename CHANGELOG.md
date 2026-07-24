# Changelog

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
