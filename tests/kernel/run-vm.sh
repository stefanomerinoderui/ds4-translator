#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Boots the running kernel in a throwaway virtme-ng VM and runs
# test-binder.sh inside it, so a kernel bug costs a VM instead of the
# desktop. Needs virtme-ng (`uv tool install virtme-ng`) and qemu.
#
#   tests/kernel/run-vm.sh
#
# The host kernel has no KASAN/lockdep; for those, build a debug kernel and
# pass its tree as KERNEL_DIR (the module is built against it).

set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD=${BUILD_DIR:-$ROOT/build/kernel-test}

command -v vng >/dev/null || { echo "virtme-ng not found: uv tool install virtme-ng" >&2; exit 2; }

KDIR=${KERNEL_DIR:-/lib/modules/$(uname -r)/build}
mkdir -p "$BUILD"
make -C "$ROOT/kernel" KDIR="$KDIR" >/dev/null
gcc -Wall -Wextra -Werror -O2 -o "$BUILD/fake-pad" "$HERE/fake-pad.c"
gcc -Wall -Wextra -Werror -O2 -o "$BUILD/pad-client" "$HERE/pad-client.c"

exec vng --run --user root --exec "$HERE/test-binder.sh $ROOT/kernel/hid-ds4-translator.ko $BUILD/fake-pad $BUILD/pad-client"
