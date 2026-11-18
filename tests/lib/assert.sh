# shellcheck shell=bash
# Shared assertion helpers for the shell test scripts. Source, don't execute.
#
# Besides pass/fail checks there is a "known issue" check, used to pin a
# documented security weakness (see docs/security-findings.md):
#   * the vulnerable behaviour is still there -> "KNOWN <id>", counts as a pass
#   * the behaviour is gone                    -> "FIXED? <id>", counts as a FAIL
# The second case is deliberate: whoever fixes the bug has to turn the check
# into a real regression test and update the register, so a finding can
# neither silently disappear nor come back.

fails=0
known=0

pass() { printf 'ok   %s\n' "$1"; }
fail() { printf 'FAIL %s\n' "$1"; fails=$((fails + 1)); }
skip() { printf 'skip %s\n' "$1"; }

# check <description> <expected-substring> <cmd...>: output must contain it.
check() {
    local desc="$1" want="$2"; shift 2
    local out
    out="$("$@" 2>&1)"
    if grep -qF -- "$want" <<<"$out"; then pass "$desc"; else fail "$desc (wanted '$want', got: $out)"; fi
}

# expect_rc <expected> <description> <cmd...>; optional EXPECT_OUT grep pattern.
expect_rc() {
    local want="$1" desc="$2"; shift 2
    local out rc
    out="$("$@" 2>&1)"; rc=$?
    if [ "$rc" -ne "$want" ]; then
        fail "$desc (exit $rc, wanted $want)"
    elif [ -n "${EXPECT_OUT:-}" ] && ! grep -Eq "$EXPECT_OUT" <<<"$out"; then
        fail "$desc (output missing /$EXPECT_OUT/)"
    else
        pass "$desc"
    fi
}

# known_issue <id> <description> <vulnerable-command...>
# The command must succeed (exit 0) exactly while the weakness exists.
known_issue() {
    local id="$1" desc="$2"; shift 2
    if "$@" >/dev/null 2>&1; then
        printf 'KNOWN %s %s\n' "$id" "$desc"
        known=$((known + 1))
    else
        printf 'FIXED? %s %s -- weakness no longer reproduces: turn this into a regression check and update docs/security-findings.md\n' "$id" "$desc"
        fails=$((fails + 1))
    fi
}

# finish <suite-name>: print the summary and exit with the right status.
finish() {
    local name="$1"
    if [ "$fails" -eq 0 ]; then
        echo "$name: all checks passed ($known known issue(s) pinned)"
    else
        echo "$name: $fails check(s) failed"
    fi
    exit "$((fails > 0))"
}
