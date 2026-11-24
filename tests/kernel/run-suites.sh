#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Runs both VM suites in one guest boot (called by run-vm.sh inside the VM) and
# fails if either does.
#
#   run-suites.sh <module> <build dir of the test tools> <ds4-translator> <ds4-ctl>

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
KO=${1:?module path}
BUILD=${2:?tool directory}
DAEMON=${3:?ds4-translator path}
DS4CTL=${4:?ds4-ctl path}

rc=0
"$HERE/test-binder.sh" "$KO" "$BUILD/fake-pad" "$BUILD/pad-client" "$BUILD/transport-check" \
    "$DAEMON" "$DS4CTL" || rc=1
"$HERE/test-virtual.sh" "$KO" "$BUILD/fake-pad" "$BUILD/virtual-check" \
    "$DAEMON" "$DS4CTL" || rc=1
exit "$rc"
