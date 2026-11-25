#!/usr/bin/env bash
# Shell completion tests: drives the real bash, fish and zsh completion scripts
# against the built ds4-ctl (make all) and checks the candidates each shell
# would offer. Needs no root, no daemon and no controller; a shell that is not
# installed is skipped.
#
# Regression guard: `ds4-ctl set-backend <TAB>` used to complete to the single
# candidate 'ds4 dualsense' in fish and zsh, because `--complete-args` printed
# every candidate on one line and those shells split command output on
# newlines only.
set -u
BUILD_DIR="${1:-build}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
# shellcheck source=../lib/assert.sh
. "$HERE/../lib/assert.sh"

if [ ! -x "$BUILD_DIR/ds4-ctl" ]; then
    fail "missing build artifact: $BUILD_DIR/ds4-ctl (run make all)"
    finish completion
fi
# The completion scripts call `ds4-ctl` by name: make that the freshly built one.
PATH="$(cd "$BUILD_DIR" && pwd):$PATH"
export PATH

BASH_SCRIPT="$ROOT/ds4-ctl-completion.bash"
FISH_SCRIPT="$ROOT/ds4-ctl.fish"
ZSH_SCRIPT="$ROOT/_ds4-ctl"

# complete_<shell> <word>...: print, one per line, the candidates the shell
# offers for the command line `ds4-ctl <word>...`, where the last word is the
# one being completed (pass "" for "right after a space").

complete_binary() {
    ds4-ctl --complete-args "${@:1:$#-1}"
}

complete_bash() {
    bash --norc --noprofile -c '
        source "$1"; shift
        COMP_WORDS=(ds4-ctl "$@")
        COMP_CWORD=$#
        _ds4_ctl_completions
        [ "${#COMPREPLY[@]}" -gt 0 ] && printf "%s\n" "${COMPREPLY[@]}"
        exit 0' _ "$BASH_SCRIPT" "$@"
}

complete_fish() {
    local line="ds4-ctl" w
    for w in "$@"; do line+=" $w"; done
    # `complete -C` prints "candidate<TAB>description"; keep the candidate.
    # shellcheck disable=SC2016 # $argv is expanded by fish, not here
    fish --no-config -c 'source $argv[1]; complete -C $argv[2]' "$FISH_SCRIPT" "$line" | cut -f1
}

complete_zsh() {
    # compadd only exists inside the completion system, so stand in for it:
    # filter the candidate array by the typed prefix, as the real one does.
    zsh -f -c '
        script=$1; shift
        words=(ds4-ctl "$@")
        CURRENT=$(( $# + 1 ))
        PREFIX=${words[CURRENT]}
        compadd() {
            [ "$1" = -a ] || return 1
            local -a cands=( "${(@P)2}" )
            print -rl -- ${(M)cands:#${PREFIX}*}
        }
        source "$script"' _ "$ZSH_SCRIPT" "$@"
}

# expect <shell> <expected-candidates> <word>...: the shell must offer exactly
# the space-separated expected candidates (order ignored), each on its own.
expect() {
    local shell="$1" want="$2"; shift 2
    local desc="$shell: ds4-ctl $* <TAB>"
    local got want_sorted
    got="$("complete_$shell" "$@" 2>&1 | sort)"
    want_sorted="$(tr ' ' '\n' <<<"$want" | sort)"
    if [ "$got" = "$want_sorted" ]; then
        pass "$desc -> $want"
    else
        fail "$desc (wanted: $want, got: $(tr '\n' '|' <<<"$got"))"
    fi
}

# expect_includes <shell> <candidate> <word>...: the shell must offer the
# candidate as a separate entry, and no entry may contain a space.
expect_includes() {
    local shell="$1" want="$2"; shift 2
    local desc="$shell: ds4-ctl $* <TAB>"
    local got
    got="$("complete_$shell" "$@" 2>&1)"
    if grep -q ' ' <<<"$got"; then
        fail "$desc (a candidate contains a space: $(tr '\n' '|' <<<"$got"))"
    elif grep -qxF -- "$want" <<<"$got"; then
        pass "$desc offers $want"
    else
        fail "$desc (wanted '$want' among: $(tr '\n' '|' <<<"$got"))"
    fi
}

run_cases() {
    local shell="$1"
    expect "$shell" "set-backend set-hide-method set-mac-squat set-name set-type" set-
    expect "$shell" "ds4 dualsense" set-backend ""
    expect "$shell" "dualsense" set-backend du
    expect "$shell" "uhid functionfs" set-backend ds4 ""
    expect "$shell" "ds4 dualsense" set-name ""
    expect "$shell" "on off" set-mac-squat ""
    expect "$shell" "legacy unbind unlink" set-hide-method ""
    expect_includes "$shell" cross tap ""
    expect_includes "$shell" xboxone set-type ""
}

# The binary's own output contract: one candidate per line. Prefix filtering
# is the shell's job, so only whole-list cases apply here.
expect binary "ds4 dualsense" set-backend ""
expect binary "uhid functionfs" set-backend ds4 ""
expect binary "on off" set-mac-squat ""

for shell in bash fish zsh; do
    if command -v "$shell" >/dev/null; then
        run_cases "$shell"
    else
        skip "$shell not installed"
    fi
done

finish completion
