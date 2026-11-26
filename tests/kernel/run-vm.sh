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
# the full set (KASAN, lockdep, kmemleak, failslab), `make kernel-test-debug`
# builds a debug kernel from tests/kernel/debug.config and passes its tree as
# KERNEL_DIR: the module is built against it and the VM boots it.
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
gcc -Wall -Wextra -Werror -O2 -o "$BUILD/virtual-check" "$HERE/virtual-check.c"
# Optional: SDL's view of the emulated pad (what most games are built on).
rm -f "$BUILD/sdl-rumble"
if pkg-config --exists sdl2 2>/dev/null; then
    # shellcheck disable=SC2046 # flags, one per word
    gcc -Wall -Wextra -Werror -O2 -o "$BUILD/sdl-rumble" "$HERE/sdl-rumble.c" $(pkg-config --cflags --libs sdl2)
fi
g++ -std=c++17 -Wall -Wextra -Werror -O2 -I"$ROOT/src" -o "$BUILD/transport-check" \
    "$HERE/transport-check.cpp" "$ROOT/src/driver-transport.cpp"

# Every suite in one VM boot (run-suites.sh); the run fails if any does. With
# KERNEL_DIR set, that tree's kernel is booted, not the host's.
RUN=(--run)
[ -n "${KERNEL_DIR:-}" ] && RUN=(--run "$KERNEL_DIR")
exec vng "${RUN[@]}" --append "slub_debug=FZPU kfence.sample_interval=1 modprobe.blacklist=hid_ds4_translator,ds4t_gadget" --user root --exec "$HERE/run-suites.sh $ROOT/kernel/hid-ds4-translator.ko $BUILD $ROOT/build/ds4-translator $ROOT/build/ds4-ctl"
