#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# The Clang static analyzer as Kbuild's "checker": `make analyze` passes this
# script as CHECK, so Kbuild runs it on every source with the module's real
# compiler flags. Kbuild is written for sparse and adds sparse-only flags
# (--arch=, -mlittle-endian) that are dropped here, and probes the checker with
# a file that has no extension, hence the explicit -x c. Findings are errors.
set -eu
args=()
for arg in "$@"; do
    case "$arg" in
        --arch=*|-mlittle-endian|-mbig-endian) ;;
        *) args+=("$arg") ;;
    esac
done
exec clang --analyze -Xclang -analyzer-output=text -Xclang -analyzer-werror \
    -x c -o /dev/null "${args[@]}"
