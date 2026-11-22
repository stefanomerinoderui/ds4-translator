#!/usr/bin/env bash
# Finds the capabilities ds4-translator.service really needs, by experiment:
# starts from a generous CapabilityBoundingSet, then drops each capability in
# turn, restarts the daemon and runs tests/hardware/verify.sh --exercise. A
# capability whose removal makes the run fail (or logs EPERM/EACCES) is needed.
#
# Needs root (it restarts the service) and a connected pad; it briefly
# interrupts the controller for each candidate (~25 s each). Everything is
# written to a runtime drop-in under /run/systemd that is removed on exit and
# disappears on reboot anyway. Run it as:   sudo tests/hardware/bisect-capabilities.sh   (or pkexec, with the full path)
#
# Covers what verify.sh covers (Bluetooth path, config writes, emulation type
# switch). USB-only needs are checked afterwards by hand with the final set.
set -u
[ "$(id -u)" -eq 0 ] || { echo "run as root (sudo or pkexec)" >&2; exit 1; }
# verify.sh runs as the invoking user: sudo sets SUDO_USER, pkexec PKEXEC_UID.
USER_NAME="${SUDO_USER:-}"
[ -n "$USER_NAME" ] || { [ -n "${PKEXEC_UID:-}" ] && USER_NAME="$(id -nu "$PKEXEC_UID")"; }
[ -n "$USER_NAME" ] || { echo "run through sudo or pkexec so verify.sh can run as your user" >&2; exit 1; }
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT=ds4-translator.service
DROPIN_DIR=/run/systemd/system/$UNIT.d
DROPIN=$DROPIN_DIR/zz-bisect.conf

# Capabilities seen in a bpftrace of cap_capable over the service's cgroup
# (SYS_ADMIN, DAC_READ_SEARCH, SYS_PTRACE, NET_ADMIN, SYS_RESOURCE, SETFCAP),
# plus the ones the code plausibly needs but a trace can miss because the
# kernel only checks a capability once plain ownership has failed.
CANDIDATES=(CAP_SYS_ADMIN CAP_BPF CAP_PERFMON CAP_DAC_OVERRIDE CAP_DAC_READ_SEARCH CAP_FOWNER
            CAP_MKNOD CAP_NET_ADMIN CAP_SYS_RESOURCE CAP_SYS_PTRACE CAP_SETFCAP)

cleanup() {
    rm -rf "$DROPIN_DIR"
    systemctl daemon-reload
    systemctl reset-failed "$UNIT" 2>/dev/null
    systemctl restart "$UNIT"
}
trap cleanup EXIT

try_set() { # $@ = capabilities to keep; returns 0 if the daemon works with exactly these
    mkdir -p "$DROPIN_DIR"
    printf '[Service]\nCapabilityBoundingSet=%s\n' "$*" >"$DROPIN"
    systemctl daemon-reload
    systemctl reset-failed "$UNIT" 2>/dev/null
    systemctl restart "$UNIT" || return 1
    sleep 6
    systemctl is-active --quiet "$UNIT" || return 1
    local inv out
    inv="$(systemctl show "$UNIT" -p InvocationID --value)"
    out="$(sudo -u "$USER_NAME" timeout 120 "$HERE/verify.sh" --exercise 2>&1)" || { echo "$out" | grep -E '^(FAIL|hardware)' | head -3; return 1; }
    if journalctl "_SYSTEMD_INVOCATION_ID=$inv" --no-pager | grep -iE 'operation not permitted|permission denied|EPERM|EACCES' | grep -v 'UDC: Permission denied' | head -2 | grep -q .; then
        journalctl "_SYSTEMD_INVOCATION_ID=$inv" --no-pager | grep -iE 'operation not permitted|permission denied|EPERM|EACCES' | head -2
        return 1
    fi
    return 0
}

echo "baseline with all ${#CANDIDATES[@]} candidates:"
try_set "${CANDIDATES[@]}" || { echo "baseline fails -- fix that first (see output above)"; exit 1; }

keep=("${CANDIDATES[@]}")
needed=()
for cap in "${CANDIDATES[@]}"; do
    trial=()
    for c in "${keep[@]}"; do [ "$c" = "$cap" ] || trial+=("$c"); done
    printf '%-22s ' "without $cap:"
    if try_set "${trial[@]}"; then
        echo "works -> not needed"
        keep=("${trial[@]}")
    else
        echo "BREAKS -> needed"
        needed+=("$cap")
    fi
done

echo
echo "CapabilityBoundingSet=${keep[*]}"
echo "(needed individually: ${needed[*]:-none})"
