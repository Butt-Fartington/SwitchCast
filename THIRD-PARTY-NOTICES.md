# Third-party notices

This file inventories code and data distributed in SwitchCast 0.3.0. It
distinguishes material inherited through SysDVR, components vendored in the
source tree, libraries statically linked by the release toolchain, and
reference-only projects.

The notices below do not replace the corresponding licenses. SwitchCast's
combined derivative source is licensed under GPL-2.0; see `LICENSE`.

## Principal foundation

### SysDVR

- Project: [SysDVR](https://github.com/exelix11/SysDVR)
- Authors: Exelix11 and the SysDVR contributors
- License: GNU General Public License version 2
- SwitchCast base snapshot:
  [`804fd36e54983b7c76b249059b159f896af35b4d`](https://github.com/exelix11/SysDVR/commit/804fd36e54983b7c76b249059b159f896af35b4d)
- Use: substantial inherited and modified capture, sysmodule, IPC, Settings UI,
  translation, packaging, and dvr-patches-management code

SysDVR is the primary foundation of SwitchCast. See
`SYSDVR-ATTRIBUTION.md` for the detailed lineage and division between the
upstream base and SwitchCast's Cast-specific work.

## Vendored source and data

The original file-level notices in these components are preserved.

| Component | Version/snapshot | Copyright or authors | License | Location |
| --- | --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.71) | 1.71 | Copyright (c) 2014-2019 Omar Cornut | MIT | `SwitchCastConfig/source/UI/imgui` |
| [GLAD](https://github.com/Dav1dde/glad/tree/v0.1.27) | generator 0.1.27 | Copyright (c) 2013-2018 David Herberth | MIT | `SwitchCastConfig/source/UI/glad.c`, `glad.h` |
| [JSON for Modern C++](https://github.com/nlohmann/json/tree/v3.11.2) | 3.11.2 | Copyright (c) 2013-2022 Niels Lohmann; embedded portions retain notices for Evan Nemerson, the Abseil Authors, Björn Hoehrmann, and Florian Loitsch | MIT; one embedded Abseil-derived portion is identified as Apache-2.0 in the header | `SwitchCastConfig/source/Libs/nlohmann/json.hpp` |
| [stb](https://github.com/nothings/stb) | stb_image 2.26; ImGui-bundled stb_rect_pack 0.99, stb_textedit 1.13, stb_truetype 1.20 | Copyright (c) 2017 Sean Barrett and contributors; ImGui's embedded ProggyClean data credits Tristan Grimmer | MIT or public domain; ProggyClean is MIT | `SwitchCastConfig/source/UI` |
| [miniz](https://github.com/richgel999/miniz) | 2.2.0 single-header snapshot | Rich Geldreich; Tenacious Software LLC; RAD Game Tools; Valve Software; Martin Raiber | MIT or public domain/Unlicense, as marked in the source | `SwitchCastConfig/source/Libs/zip/miniz.h` |
| [kuba--/zip](https://github.com/kuba--/zip) | SysDVR snapshot; exact upstream version not encoded in the files | Upstream kuba--/zip authors | MIT | `SwitchCastConfig/source/Libs/zip/zip.c`, `zip.h` |
| [nanoprintf](https://github.com/charlesnicholson/nanoprintf) | SysDVR snapshot | Copyright (c) 2019- Charles Nicholson | 0BSD or Unlicense | `sysmodule/source/third_party` |
| Open Sans Regular | subset of the Apache-2.0-era font | Digitized data copyright (c) 2010-2011 Google Corporation; designed by Steve Matteson | Apache-2.0 | `SwitchCastConfig/romfs/fonts/opensans.ttf` |
| [Noto Sans SC](https://github.com/notofonts/noto-cjk) and [Noto Sans TC](https://github.com/notofonts/noto-cjk) | regular subsets | Copyright 2014-2021 Adobe, with Reserved Font Name "Source" | SIL Open Font License 1.1 | `SwitchCastConfig/romfs/fonts/NotoSansSC-Regular.ttf`, `NotoSansTC-Regular.ttf` |

The font license texts required for redistribution are included under
`third_party/licenses/`.

### MIT notice for Dear ImGui

Copyright (c) 2014-2019 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT notice for GLAD

Copyright (c) 2013-2018 David Herberth

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT notice for JSON for Modern C++

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT notice for stb

Copyright (c) 2017 Sean Barrett

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT notice for the ProggyClean data embedded by Dear ImGui

Copyright (c) 2004-2005 Tristan Grimmer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT notice for miniz

Copyright 2013-2014 RAD Game Tools and Valve Software

Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

Copyright 2016 Martin Raiber

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### MIT notice for kuba--/zip

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### 0BSD notice for nanoprintf

Copyright (c) 2019- Charles Nicholson

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

## Statically linked release dependencies

The official 0.3.0 binaries were built with the following devkitPro packages.
Their source is not vendored in this repository, but portions are linked into
the executable as shown by the release link map.

| Package/component | Release build version | Authors/license | Source |
| --- | --- | --- | --- |
| libnx | 4.12.0-1 | Copyright 2017-2018 libnx Authors; ISC | [switchbrew/libnx](https://github.com/switchbrew/libnx) |
| libcurl | switch-curl 7.69.1-5 | Daniel Stenberg and contributors; curl license | [curl/curl 7.69.1](https://github.com/curl/curl/tree/curl-7_69_1) |
| zlib | switch-zlib 1.3.1-1 | Jean-loup Gailly and Mark Adler; zlib license | [madler/zlib 1.3.1](https://github.com/madler/zlib/tree/v1.3.1) |
| GLFW | switch-glfw 3.3.4-2 | Marcus Geelnard and Camilla Löwy; zlib/libpng license | [glfw/glfw 3.3.4](https://github.com/glfw/glfw/tree/3.3.4) |
| Mesa EGL/glapi | switch-mesa 20.1.0-5 | Mesa authors and contributors; primarily MIT-style licenses | [Mesa](https://gitlab.freedesktop.org/mesa/mesa) and [devkitPro patch](https://github.com/devkitPro/pacman-packages/tree/master/switch/mesa) |
| libdrm_nouveau | switch-libdrm_nouveau 1.0.1-2 | Stephane Marchesin; Red Hat Inc.; Tungsten Graphics; Ben Skeggs and contributors; MIT | [devkitPro/libdrm_nouveau](https://github.com/devkitPro/libdrm_nouveau) |
| GCC runtime, libstdc++, newlib, and devkitA64 system runtime | GCC 16.1.0; newlib 4.6.0 | Free Software Foundation, newlib contributors, and devkitPro contributors; GCC Runtime Library Exception and component-specific permissive licenses | [GCC](https://gcc.gnu.org/), [newlib](https://sourceware.org/newlib/), [devkitPro](https://devkitpro.org/) |

### libnx ISC notice

Copyright 2017-2018 libnx Authors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

### libcurl notice

Copyright (c) 1996-2020 Daniel Stenberg and many contributors.

All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization of the copyright holder.

### zlib notice

Copyright (c) 1995-2022 Jean-loup Gailly and Mark Adler

This software is provided "as-is", without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use
of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

### GLFW notice

Copyright (c) 2002-2006 Marcus Geelnard

Copyright (c) 2006-2019 Camilla Löwy

This software is provided "as-is", without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use
of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

### Mesa default MIT notice

Copyright (c) 1999-2007 Brian Paul. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### libdrm_nouveau MIT notice

Copyright 2005 Stephane Marchesin

Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND, USA

Copyright 2012 Red Hat Inc.

Authors include Ben Skeggs.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Integrated or reference-only projects

These projects are not copied into the SwitchCast binary:

- [Open Screen libcast](https://chromium.googlesource.com/openscreen/+/refs/heads/main/cast/)
  (Chromium/Open Screen authors, BSD-3-Clause) was used as a public protocol
  reference while implementing the Cast Streaming transport.
- [node-castv2-client](https://github.com/thibauts/node-castv2-client)
  (Copyright 2014 Thibaut Séguy, MIT) was consulted for Cast V2
  control-channel namespace and session behavior. Its JavaScript source is not
  vendored or linked into SwitchCast.
- [dvr-patches](https://github.com/exelix11/dvr-patches) (Exelix11 and
  contributors, BSD-3-Clause) is downloaded and managed as a separate optional
  runtime project by the Settings UI.
- Google Cast, Chromecast, Nintendo Switch, Atmosphère, and related names and
  protocols remain the property of their respective owners. Their mention
  describes compatibility and does not imply endorsement.
