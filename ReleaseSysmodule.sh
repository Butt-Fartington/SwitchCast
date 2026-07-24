#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
VERSION=0.3.0

if [ -z "${DEVKITPRO:-}" ]; then
	echo "Set DEVKITPRO to a complete devkitPro installation." >&2
	exit 1
fi

make -C "$PROJECT_ROOT/sysmodule" clean
make -C "$PROJECT_ROOT/sysmodule" -j
make -C "$PROJECT_ROOT/SwitchCastConfig" clean
make -C "$PROJECT_ROOT/SwitchCastConfig" -j

HOST_BUILD="$PROJECT_ROOT/build-host"
mkdir -p "$HOST_BUILD"

cc -std=c11 -Wall -Wextra -Werror -O2 \
	"$PROJECT_ROOT/sysmodule/source/cast/cast_streaming.c" \
	"$PROJECT_ROOT/tests/test_cast_streaming.c" \
	-o "$HOST_BUILD/test_cast_streaming"
"$HOST_BUILD/test_cast_streaming"

cc -std=c11 -Wall -Wextra -Werror -O2 \
	"$PROJECT_ROOT/sysmodule/source/cast/cast_proto.c" \
	"$PROJECT_ROOT/tests/test_cast_proto.c" \
	-o "$HOST_BUILD/test_cast_proto"
"$HOST_BUILD/test_cast_proto"

cc -std=c11 -Wall -Wextra -Werror -O2 \
	"$PROJECT_ROOT/sysmodule/source/cast/fmp4.c" \
	"$PROJECT_ROOT/tests/test_fmp4.c" \
	-o "$HOST_BUILD/test_fmp4"
"$HOST_BUILD/test_fmp4"

c++ -std=c++17 -Wall -Wextra -Werror -O2 \
	"$PROJECT_ROOT/SwitchCastConfig/source/CastDiscovery.cpp" \
	"$PROJECT_ROOT/tests/test_cast_discovery.cpp" \
	-o "$HOST_BUILD/test_cast_discovery"
"$HOST_BUILD/test_cast_discovery"

RELEASE_ROOT="$PROJECT_ROOT/build-release/SwitchCast"
rm -rf "$PROJECT_ROOT/build-release"
mkdir -p \
	"$RELEASE_ROOT/atmosphere/contents/00FF000053434153/flags" \
	"$RELEASE_ROOT/config/switchcast" \
	"$RELEASE_ROOT/switch"
cp -R "$PROJECT_ROOT/release-template/." "$RELEASE_ROOT/"

cp "$PROJECT_ROOT/sysmodule/sysmodule.nsp" \
	"$RELEASE_ROOT/atmosphere/contents/00FF000053434153/exefs.nsp"
cp "$PROJECT_ROOT/sysmodule/toolbox.json" \
	"$RELEASE_ROOT/atmosphere/contents/00FF000053434153/toolbox.json"
cp "$PROJECT_ROOT/SwitchCastConfig/SwitchCast.nro" \
	"$RELEASE_ROOT/switch/SwitchCast.nro"
cp "$PROJECT_ROOT/README.md" "$RELEASE_ROOT/SWITCHCAST.md"
cp "$PROJECT_ROOT/SYSDVR-ATTRIBUTION.md" "$RELEASE_ROOT/"
cp "$PROJECT_ROOT/CASTING.md" "$RELEASE_ROOT/SWITCHCAST-PROTOCOL.md"
cp "$PROJECT_ROOT/BRANDING.md" "$RELEASE_ROOT/"
cp "$PROJECT_ROOT/NOTICE.md" "$PROJECT_ROOT/THIRD-PARTY-NOTICES.md" \
	"$PROJECT_ROOT/LICENSE" "$RELEASE_ROOT/"
mkdir -p "$RELEASE_ROOT/third_party/licenses"
cp "$PROJECT_ROOT/third_party/licenses/"* \
	"$RELEASE_ROOT/third_party/licenses/"

(
	cd "$RELEASE_ROOT"
	zip -q -r "../SwitchCast-Standalone-v$VERSION.zip" .
)

echo "Built $PROJECT_ROOT/build-release/SwitchCast-Standalone-v$VERSION.zip"
