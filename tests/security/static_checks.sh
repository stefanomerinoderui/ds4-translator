#!/usr/bin/env bash
# Static security checks over the repo and the built binaries: hardening flags,
# the systemd unit, udev rules, CI workflows, shell scripts and leaked secrets.
# No daemon, no root. Tools that are not installed are skipped, not failed.
#
# "known_issue" checks pin documented weaknesses (docs/security-findings.md):
# they pass while the weakness exists and fail with "FIXED?" once it is gone.
set -u
BUILD_DIR="${1:-build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$ROOT/tests"
# shellcheck source=../lib/assert.sh
. "$HERE/lib/assert.sh"
cd "$ROOT" || exit 2

have() { command -v "$1" >/dev/null 2>&1; }

# ------------------------------------------------------- L6: binary hardening
# Whether a toolchain hardens by default varies (Ubuntu does, Arch only
# partly), so the build must ask for it explicitly and the result must show.
require "Makefile requests _FORTIFY_SOURCE" grep -q '_FORTIFY_SOURCE' Makefile
require "Makefile requests the stack protector" grep -q 'fstack-protector-strong' Makefile
require "Makefile links PIE" grep -q -- '-pie' Makefile
require "Makefile requests full RELRO (-z now)" grep -q 'z,relro,-z,now' Makefile

# hardening_status <binary> <PIE|RELRO|BIND_NOW|CANARY|FORTIFY>
# shellcheck disable=SC2317,SC2329 # run by name through require, not called directly (SC2317 before shellcheck 0.10)
hardening_status() {
    local bin="$1"
    case "$2" in
        PIE)      readelf -h "$bin" 2>/dev/null | grep -q 'Type:.*DYN' ;;
        RELRO)    readelf -lW "$bin" 2>/dev/null | grep -q GNU_RELRO ;;
        BIND_NOW) readelf -dW "$bin" 2>/dev/null | grep -qE 'BIND_NOW|FLAGS.*NOW' ;;
        CANARY)   nm -D "$bin" 2>/dev/null | grep -q __stack_chk_fail ;;
        FORTIFY)  nm -D "$bin" 2>/dev/null | grep -q '__.*_chk' ;;
    esac
}

if have readelf && have nm; then
    for b in ds4-translator ds4-ctl; do
        [ -e "$BUILD_DIR/$b" ] || continue
        for feature in PIE RELRO BIND_NOW CANARY FORTIFY; do
            require "$b is built with $feature" hardening_status "$BUILD_DIR/$b" "$feature"
        done
    done
    # The shim has no stack arrays, so a canary is not expected there.
    for feature in PIE RELRO BIND_NOW; do
        [ -e "$BUILD_DIR/libudev-sony-spoof.so" ] &&
            require "libudev-sony-spoof.so is built with $feature" hardening_status "$BUILD_DIR/libudev-sony-spoof.so" "$feature"
    done
else
    skip "hardening checks (readelf/nm missing)"
fi

# -------------------------------------------------- L5: systemd unit hardening
unit=ds4-translator.service
for directive in NoNewPrivileges ProtectSystem ProtectHome PrivateTmp RestrictAddressFamilies LockPersonality SystemCallFilter CapabilityBoundingSet; do
    require "$unit sets $directive" grep -q "^$directive=" "$unit"
done
if have systemd-analyze; then
    echo "SYSTEMD exposure: $(systemd-analyze security --offline=true "$unit" 2>/dev/null | tail -1)"
fi

# ------------------------------------------------------------ L4: udev rules
rules=72-ds4-translator-hide.rules
require "hiding rules lock physical nodes to root (0600)" grep -q 'MODE="0600"' "$rules"
require "no udev rule makes a node world-accessible (MODE 0666)" bash -c "! grep -q 'MODE=\"0666\"' $rules"

# ------------------------------------------------------ L7: CI supply chain
workflow_files=$(ls .github/workflows/*.yml .github/actions/*/action.yml)
# shellcheck disable=SC2317,SC2329 # run by name through require, not called directly (SC2317 before shellcheck 0.10)
unpinned() {
    # shellcheck disable=SC2086 # intentional word splitting over the file list
    ! grep -hE '^\s*-?\s*uses:\s+[^.[:space:]]' $workflow_files | grep -vE '@[0-9a-f]{40}(\s|$)' | grep -q .
}
require "every workflow action is pinned to a commit SHA" unpinned
require "bpftool download is checksum-verified" \
    bash -c "grep -q 'sha256sum -c' .github/actions/setup-deps/action.yml"
require "every workflow declares permissions:" \
    bash -c "for f in .github/workflows/*.yml; do grep -q '^permissions:' \$f || exit 1; done"
require "the signing key is not available to the build step" \
    bash -c "! awk '/name: Build packages\$/,/name: Sign packages/' .github/workflows/release.yml | grep -q ARCH_GPG_PRIVATE_KEY"

if have actionlint; then
    if actionlint; then pass "actionlint: workflows are well-formed"; else fail "actionlint: workflows are well-formed"; fi
else
    skip "actionlint not installed"
fi
if have zizmor; then
    # Any finding fails, whatever its severity; zizmor suppresses nothing here.
    if zizmor --no-progress .github >"$BUILD_DIR/zizmor.txt" 2>&1; then
        pass "zizmor: workflows have no findings"
    else
        grep -E '^(error|warning|info|note)' "$BUILD_DIR/zizmor.txt" | head -20
        fail "zizmor: workflows have no findings (see $BUILD_DIR/zizmor.txt)"
    fi
else
    skip "zizmor not installed"
fi

# ------------------------------------------------------------ shell scripts
if have shellcheck; then
    # shellcheck disable=SC2046 # file list, one path per word
    # Every severity, notes and style included: the scripts are held to zero.
    if shellcheck -x -P SCRIPTDIR rebind-unbound-controllers.sh tools/ds4-intercept/build.sh \
        $(git ls-files 'tests/*.sh') tests/lib/assert.sh tests/lib/daemon.sh; then
        pass "shellcheck: scripts are clean"
    else
        fail "shellcheck: scripts are clean"
    fi
else
    skip "shellcheck not installed"
fi

# ------------------------------------------------------------ leaked secrets
if have gitleaks; then
    if gitleaks detect --no-banner --redact >/dev/null 2>&1; then
        pass "gitleaks: no secrets in history"
    else
        fail "gitleaks: no secrets in history"
    fi
else
    skip "gitleaks not installed"
fi
if git ls-files | grep -qE '(^|/)\.signing/|\.(key|pem|gpg)$|id_rsa'; then
    fail "no key material tracked by git"
else
    pass "no key material tracked by git"
fi

finish security-static
