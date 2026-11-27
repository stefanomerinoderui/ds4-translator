#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Runs the VM suites (called by run-vm.sh inside the VM) and fails if any
# does: all of them, or only the one named.
#
#   run-suites.sh <module> <build dir of the test tools> <ds4-translator> <ds4-ctl> [suite]

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
KO=${1:?module path}
BUILD=${2:?tool directory}
DAEMON=${3:?ds4-translator path}
DS4CTL=${4:?ds4-ctl path}
ONLY=${5:-}

# Whether suite $1 is to run.
wanted() { [ -z "$ONLY" ] || [ "$ONLY" = "$1" ]; }

rc=0
if wanted binder; then
    "$HERE/test-binder.sh" "$KO" "$BUILD/fake-pad" "$BUILD/pad-client" "$BUILD/transport-check" \
        "$DAEMON" "$DS4CTL" || rc=1
fi
for suite in virtual hotswap gadget faults; do
    # faults needs a debug kernel (it skips itself otherwise) and fails
    # allocations, so it comes last.
    wanted "$suite" || continue
    "$HERE/test-$suite.sh" "$KO" "$BUILD/fake-pad" "$BUILD/virtual-check" \
        "$DAEMON" "$DS4CTL" || rc=1
done
exit "$rc"
