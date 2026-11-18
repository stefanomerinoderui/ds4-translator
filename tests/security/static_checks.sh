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
# Whether a given toolchain hardens by default varies (Ubuntu does, Arch only
# partly), so the build must ask for it explicitly. The measured result for
# each artifact is printed either way.
known_issue L6 "Makefile does not request _FORTIFY_SOURCE" bash -c "! grep -q '_FORTIFY_SOURCE' Makefile"
known_issue L6 "Makefile does not request -fstack-protector-strong" bash -c "! grep -q 'stack-protector' Makefile"
known_issue L6 "Makefile does not request PIE / full RELRO" bash -c "! grep -qE -- '-pie|-fPIE|z,now' Makefile"

hardening_report() {
    local bin="$1" pie relro now canary fortify
    pie=$(readelf -h "$bin" 2>/dev/null | grep -q 'Type:.*DYN' && echo yes || echo NO)
    relro=$(readelf -lW "$bin" 2>/dev/null | grep -q GNU_RELRO && echo yes || echo NO)
    now=$(readelf -dW "$bin" 2>/dev/null | grep -qE 'BIND_NOW|FLAGS.*NOW' && echo yes || echo NO)
    canary=$(nm -D "$bin" 2>/dev/null | grep -q __stack_chk_fail && echo yes || echo NO)
    fortify=$(nm -D "$bin" 2>/dev/null | grep -q '__.*_chk' && echo yes || echo NO)
    printf 'HARDENING %-26s PIE=%s RELRO=%s BIND_NOW=%s CANARY=%s FORTIFY=%s\n' \
        "$(basename "$bin")" "$pie" "$relro" "$now" "$canary" "$fortify"
}
if have readelf && have nm; then
    for b in ds4-translator ds4-ctl libudev-sony-spoof.so; do
        [ -e "$BUILD_DIR/$b" ] && hardening_report "$BUILD_DIR/$b"
    done
else
    skip "hardening report (readelf/nm missing)"
fi

# -------------------------------------------------- L5: systemd unit hardening
unit=ds4-translator.service
for directive in NoNewPrivileges ProtectSystem ProtectHome PrivateTmp CapabilityBoundingSet \
                 RestrictAddressFamilies SystemCallFilter; do
    known_issue L5 "$unit lacks $directive" bash -c "! grep -q '^$directive=' $unit"
done
if have systemd-analyze; then
    echo "SYSTEMD exposure: $(systemd-analyze security --offline=true "$unit" 2>/dev/null | tail -1)"
fi

# ------------------------------------------------------------ L4: udev rules
rules=72-ds4-translator-hide.rules
grep -q 'MODE="0600"' "$rules" && pass "hiding rules lock physical nodes to root (0600)" \
    || fail "hiding rules lock physical nodes to root (0600)"
known_issue L4 "virtual controller hidraw nodes are world-accessible (MODE=0666)" grep -q 'MODE="0666"' "$rules"

# ------------------------------------------------------ L7: CI supply chain
workflow_files=$(ls .github/workflows/*.yml .github/actions/*/action.yml)
unpinned() {
    # shellcheck disable=SC2086 # intentional word splitting over the file list
    grep -hE '^\s*-?\s*uses:\s*[^.]' $workflow_files | grep -vE '@[0-9a-f]{40}(\s|$)' | grep -q .
}
known_issue L7 "workflow actions are pinned by tag, not commit SHA" unpinned
# shellcheck disable=SC2086
known_issue L7 "bpftool is downloaded without a checksum (setup-deps action)" \
    bash -c "grep -q 'releases/download/.*bpftool' .github/actions/setup-deps/action.yml && ! grep -q 'sha256sum' .github/actions/setup-deps/action.yml"
known_issue L7 "lint.yml has no permissions: block" bash -c "! grep -q '^permissions:' .github/workflows/lint.yml"
grep -q '^permissions:' .github/workflows/tests.yml && pass "tests.yml declares least-privilege permissions" \
    || fail "tests.yml declares least-privilege permissions"

if have actionlint; then
    actionlint && pass "actionlint: workflows are well-formed" || fail "actionlint: workflows are well-formed"
else
    skip "actionlint not installed"
fi
if have zizmor; then
    zizmor --no-progress .github >"$BUILD_DIR/zizmor.txt" 2>&1
    echo "ZIZMOR findings: $(grep -c '^\(error\|warning\)' "$BUILD_DIR/zizmor.txt") (see $BUILD_DIR/zizmor.txt)"
else
    skip "zizmor not installed"
fi

# ------------------------------------------------------------ shell scripts
if have shellcheck; then
    # shellcheck disable=SC2046 # file list, one path per word
    shellcheck -x -P SCRIPTDIR --severity=warning rebind-unbound-controllers.sh tools/ds4-intercept/build.sh \
        $(git ls-files 'tests/*.sh') tests/lib/assert.sh tests/lib/daemon.sh \
        && pass "shellcheck: scripts are clean" || fail "shellcheck: scripts are clean"
else
    skip "shellcheck not installed"
fi

# ------------------------------------------------------------ leaked secrets
if have gitleaks; then
    gitleaks detect --no-banner --redact >/dev/null 2>&1 && pass "gitleaks: no secrets in history" \
        || fail "gitleaks: no secrets in history"
else
    skip "gitleaks not installed"
fi
git ls-files | grep -qE '(^|/)\.signing/|\.(key|pem|gpg)$|id_rsa' \
    && fail "no key material tracked by git" || pass "no key material tracked by git"

finish security-static
