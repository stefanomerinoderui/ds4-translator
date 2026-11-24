#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Boots the running kernel in a throwaway virtme-ng VM and runs
# test-binder.sh inside it, so a kernel bug costs a VM instead of the
# desktop. Needs virtme-ng (`uv tool install virtme-ng`) and qemu.
#
#   tests/kernel/run-vm.sh
#
# The host kernel has no KASAN or lockdep, so the VM boots it with SLUB
# poisoning and red zones (slub_debug=FZPU) and aggressive KFENCE sampling,
# which catch most use-after-free and out-of-bounds writes in slab memory. For
# the full set, build a debug kernel and pass its tree as KERNEL_DIR (the
# module is built against it).
#
# The guest shares the host's /lib/modules, so a DKMS-installed copy of the
# module would be autoloaded by the first DS4-shaped device and stand in for the
# build under test; modprobe.blacklist keeps alias autoload off (insmod by path
# is unaffected).

set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD=${BUILD_DIR:-$ROOT/build/kernel-test}

command -v vng >/dev/null || { echo "virtme-ng not found: uv tool install virtme-ng" >&2; exit 2; }

KDIR=${KERNEL_DIR:-/lib/modules/$(uname -r)/build}
mkdir -p "$BUILD"
make -C "$ROOT/kernel" KDIR="$KDIR" >/dev/null
make -C "$ROOT" build/ds4-translator build/ds4-ctl >/dev/null
gcc -Wall -Wextra -Werror -O2 -o "$BUILD/fake-pad" "$HERE/fake-pad.c"
gcc -Wall -Wextra -Werror -O2 -o "$BUILD/pad-client" "$HERE/pad-client.c"
g++ -std=c++17 -Wall -Wextra -Werror -O2 -I"$ROOT/src" -o "$BUILD/transport-check" \
    "$HERE/transport-check.cpp" "$ROOT/src/driver-transport.cpp"

exec vng --run --append "slub_debug=FZPU kfence.sample_interval=1 modprobe.blacklist=hid_ds4_translator" --user root --exec "$HERE/test-binder.sh $ROOT/kernel/hid-ds4-translator.ko $BUILD/fake-pad $BUILD/pad-client $BUILD/transport-check $ROOT/build/ds4-translator $ROOT/build/ds4-ctl"
