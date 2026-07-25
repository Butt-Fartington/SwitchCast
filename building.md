# Building SwitchCast

Install devkitPro with devkitA64, libnx, and the Switch portlibs needed by the
Settings app: curl, zlib, GLFW, EGL, and their graphics dependencies.

The official 0.3.x releases use:

```text
devkitA64 / GCC 16.1.0
libnx 4.12.0-1
switch-curl 7.69.1-5
switch-zlib 1.3.1-1
switch-glfw 3.3.4-2
switch-mesa 20.1.0-5
switch-libdrm_nouveau 1.0.1-2
```

```sh
export DEVKITPRO=/opt/devkitpro
make -C sysmodule -j
make -C SwitchCastConfig -j
```

Outputs:

```text
sysmodule/sysmodule.nsp
SwitchCastConfig/SwitchCast.nro
```

Run the host-side Cast checks:

```sh
mkdir -p build-host
cc -std=c11 -Wall -Wextra -Werror -O2 \
  sysmodule/source/cast/frame_arena.c \
  tests/test_frame_arena.c \
  -o build-host/test_frame_arena
./build-host/test_frame_arena

cc -std=c11 -Wall -Wextra -Werror -O2 \
  sysmodule/source/cast/cast_streaming.c \
  tests/test_cast_streaming.c \
  -o build-host/test_cast_streaming
./build-host/test_cast_streaming

cc -std=c11 -Wall -Wextra -Werror -O2 \
  sysmodule/source/cast/cast_proto.c \
  tests/test_cast_proto.c \
  -o build-host/test_cast_proto
./build-host/test_cast_proto

cc -std=c11 -Wall -Wextra -Werror -O2 \
  sysmodule/source/cast/fmp4.c \
  tests/test_fmp4.c \
  -o build-host/test_fmp4
./build-host/test_fmp4

c++ -std=c++17 -Wall -Wextra -Werror -O2 \
  SwitchCastConfig/source/CastDiscovery.cpp \
  tests/test_cast_discovery.cpp \
  -o build-host/test_cast_discovery
./build-host/test_cast_discovery
```

Release layout:

```text
atmosphere/contents/00FF000053434153/exefs.nsp
atmosphere/contents/00FF000053434153/flags/boot2.flag
atmosphere/contents/00FF000053434153/toolbox.json
switch/SwitchCast.nro
```

`ReleaseSysmodule.sh` performs a clean two-binary build, runs the host checks,
assembles the SD-card layout, includes all attribution and license documents,
and writes `build-release/SwitchCast-Standalone-v0.3.2.zip`.
