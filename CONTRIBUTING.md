# Contributing

SwitchCast welcomes focused fixes, receiver compatibility reports, tests, and
documentation improvements.

## Before opening a pull request

1. Build both the sysmodule and Settings NRO with devkitPro.
2. Run `ReleaseSysmodule.sh`; it rebuilds the binaries and runs the host-side
   Cast protocol, streaming, fMP4/H.264 configuration, and discovery checks.
3. Test title launch and exit, receiver reconnect, ultra-low and stable modes,
   and an extended gameplay session when the change affects transport state.
4. Keep audio out of unrelated video changes; the current release deliberately
   spends its memory and latency budget on video.

## Attribution and licensing

SwitchCast is a GPL-2.0 derivative of SysDVR. Contributions must be compatible
with GPL-2.0 and must not remove or diminish upstream SysDVR notices.

When adding third-party code or data:

- preserve its copyright and license headers;
- add it to `THIRD-PARTY-NOTICES.md`;
- include any standalone license text required in binary distributions; and
- identify whether the material is vendored, linked, downloaded at runtime, or
  used only as a protocol reference.

Do not submit proprietary SDK material, console keys, receiver credentials, or
private Cast application IDs.
