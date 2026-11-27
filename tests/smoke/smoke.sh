#!/usr/bin/env bash
# Smoke test: is the build usable at all? Runs against already-built binaries
# (make all), needs no root, no daemon and no controller.
set -u
BUILD_DIR="${1:-build}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib/assert.sh
. "$HERE/../lib/assert.sh"

for bin in ds4-translator ds4-ctl libudev-sony-spoof.so hid-bpf-transport.bpf.o; do
    if [ -e "$BUILD_DIR/$bin" ]; then pass "built: $bin"; else fail "missing build artifact: $bin"; fi
done

EXPECT_OUT='^ds4-translator ' expect_rc 0 "daemon --version" "$BUILD_DIR/ds4-translator" --version
EXPECT_OUT='Usage' expect_rc 0 "daemon --help" "$BUILD_DIR/ds4-translator" --help
EXPECT_OUT='Unknown type' expect_rc 1 "daemon rejects bad -t" "$BUILD_DIR/ds4-translator" -t bogus
EXPECT_OUT='^ds4-ctl ' expect_rc 0 "ctl version" "$BUILD_DIR/ds4-ctl" version
EXPECT_OUT='status' expect_rc 0 "ctl --commands lists status" "$BUILD_DIR/ds4-ctl" --commands
EXPECT_OUT='Unknown command' expect_rc 1 "ctl rejects unknown command" "$BUILD_DIR/ds4-ctl" bogus
EXPECT_OUT='Invalid type' expect_rc 1 "ctl validates set-type before connecting" "$BUILD_DIR/ds4-ctl" set-type nope
EXPECT_OUT='0-255' expect_rc 1 "ctl validates led range before connecting" "$BUILD_DIR/ds4-ctl" led 300 0 0 0 0

# The spoof shim must export the libudev entry points it overrides.
if nm -D "$BUILD_DIR/libudev-sony-spoof.so" | grep -q ' T udev_device_get_'; then
    pass "spoof lib exports udev_device_get_*"
else
    fail "spoof lib exports udev_device_get_*"
fi

# The BPF object must be a BPF ELF, not a host object.
if command -v readelf >/dev/null && readelf -h "$BUILD_DIR/hid-bpf-transport.bpf.o" | grep -q 'Linux BPF'; then
    pass "BPF object has BPF machine type"
else
    fail "BPF object has BPF machine type"
fi

finish smoke
