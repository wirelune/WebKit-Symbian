#!/usr/bin/env bash
# Builds wkengine.exe, the WebView as a server process for the Qt browser
# (qtbrowser/), after build-webcore.sh. See engine/EngineProtocol.h.
. "$(dirname "$0")/common.sh"
require_sdk
build=$ENGINE_BUILD
"$SDK/bin/cmake" -S "$REPO/engine" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/symbian-arm.cmake" -DSYMBIAN_SDK_PREFIX="$SDK" \
  -DCMAKE_MAKE_PROGRAM="$SDK/bin/ninja" -DCMAKE_BUILD_TYPE=Release \
  -DSYMBIAN_NATIVE_CONVERTER="$SYMBIAN_NATIVE" \
  -DWEBKIT_SOURCE_DIR="$WEBKIT_SRC" -DWEBKIT_BUILD_DIR="$WEBCORE_BUILD" \
  -DDEPS_DIR="$DEPS_DIR/install" -DICU_LIBRARY_DIR="$ICU_DIR/install/lib" \
  > "$WORK/engine-configure.log"
"$SDK/bin/ninja" -C "$build" > "$WORK/engine-build.log"
# The browser's heap: WebKit needs far more than the default 1 MB.
python3 "$REPO/scripts/e32-header.py" "$build/e32/wkengine.exe" \
  "${ENGINE_HEAP_MIN:-0x100000}" "${ENGINE_HEAP_MAX:-0x8000000}" "${ENGINE_STACK:-0x100000}"
ls -l "$build/e32/"
