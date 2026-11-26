#!/usr/bin/env bash
# Lists where the port's sources depend on Symbian: OS(SYMBIAN) and
# __SYMBIAN32__, outside the Symbian-only directories. Each site is either
# behaviour of the port (becomes PLATFORM(SYMBIAN), built on the test host
# too) or a Symbian API call (stays OS(SYMBIAN)).
set -euo pipefail
cd "$(dirname "$0")/../../.."
grep -rnE 'OS\(SYMBIAN\)|__SYMBIAN32__' Source \
  --include='*.h' --include='*.cpp' --include='*.c' --include='*.cmake' --include='*.txt' \
  | grep -vE '^Source/(WTF/wtf/symbian|WebCore/platform/symbian|WebKitLegacy/symbian)/' \
  | sort
