---
name: Bug report
about: Report a reproducible SwitchCast problem
title: "[Bug] "
labels: bug
assignees: ""
---

Before opening an issue, confirm the problem occurs with an unmodified
SwitchCast release and that SysDVR or another capture sysmodule is not running
at the same time.

## What happened?

Describe the behavior, what you expected, and the shortest sequence that
reproduces it.

## Environment

- SwitchCast version:
- Console firmware:
- Atmosphère version:
- Receiver/TV model:
- Network layout (router/AP, 2.4 or 5 GHz, guest/VLAN if relevant):
- Latency profile (`ultra` or `stable`):
- Game or homebrew title:
- dvr-patches installed: yes/no

## Diagnostics

Attach `/config/switchcast/debug.json`,
`/config/switchcast/last-failure.json`, and
`/config/switchcast/error.json` when present. Remove anything you consider
private before posting.

## Additional context

Include photos, receiver behavior, approximate run time before failure, and
whether returning to HOME or changing titles was involved.
