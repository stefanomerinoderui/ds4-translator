#!/usr/bin/env bash
# Local rehearsal of the release workflow's `arch-repo` job (no tag, no
# publishing): builds the packages in a real Arch container, signs them with a
# throwaway key using the same commands as .github/workflows/release.yml, then
# installs them from the resulting repository with pacman enforcing
# `SigLevel = Required`, and checks pacman rejects tampered, unsigned and
# foreign-key repositories.
#
# Needs docker (no root otherwise); uses the host network because the default
# bridge has no DNS on some setups. Builds the committed HEAD, so commit first.
# Keep the steps in sync with the arch-repo job.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/ds4-release-sim.XXXXXX")"
IMAGE="${ARCH_IMAGE:-archlinux:base-devel}"
trap 'rm -rf "$OUT"' EXIT

# A plain clone: in a git worktree .git is a file pointing outside the mount.
git clone -q --no-hardlinks "$ROOT" "$OUT/src"

echo "== build + sign in $IMAGE (output: $OUT)"
docker run --rm --network host -v "$OUT/src:/repo:ro" -v "$OUT:/out" "$IMAGE" bash -euo pipefail -c '
    printf "\n[multilib]\nInclude = /etc/pacman.d/mirrorlist\n" >> /etc/pacman.conf
    pacman -Syu --noconfirm --needed git clang bpf cmake qt6-base qt6-declarative qt6-svg \
        lib32-glibc lib32-gcc-libs libusb libbpf systemd kmod >/dev/null
    useradd -m builder
    git config --global --add safe.directory /repo/.git
    git clone -q /repo /work && chown -R builder /work && cd /work
    git config --global --add safe.directory "$PWD"
    runuser -u builder -- git config --global --add safe.directory "$PWD"
    runuser -u builder -- make arch-pkg >/dev/null

    mkkey() { # $1 = home dir, $2 = uid email
        runuser -u builder -- env GNUPGHOME="$1" gpg --batch --pinentry-mode loopback --passphrase "" \
            --quick-generate-key "$2" ed25519 sign never 2>/dev/null
    }
    for name in release other; do
        install -d -o builder -m 700 "/tmp/gpg-$name"
        mkkey "/tmp/gpg-$name" "ds4 $name <$name@example.invalid>"
    done
    fpr=$(runuser -u builder -- env GNUPGHOME=/tmp/gpg-release gpg --list-secret-keys --with-colons | awk -F: "/^fpr/ {print \$10; exit}")
    other=$(runuser -u builder -- env GNUPGHOME=/tmp/gpg-other gpg --list-secret-keys --with-colons | awk -F: "/^fpr/ {print \$10; exit}")

    # Same signing step as the workflow, against the throwaway key.
    for pkg in build/arch/*.pkg.tar.zst; do
        runuser -u builder -- env GNUPGHOME=/tmp/gpg-release gpg --batch --yes --local-user "$fpr" --detach-sign --no-armor "$pkg"
    done
    runuser -u builder -- env GNUPGHOME=/tmp/gpg-release make arch-repo ARCH_SIGN_KEY="$fpr"

    mkdir /out/site /out/pub
    cd build/arch
    cp *.pkg.tar.zst ds4-translator.db ds4-translator.files /out/site/
    cp *.pkg.tar.zst.sig ds4-translator.db.sig ds4-translator.files.sig /out/site/
    runuser -u builder -- env GNUPGHOME=/tmp/gpg-release gpg --armor --export "$fpr" > /out/pub/release.asc
    runuser -u builder -- env GNUPGHOME=/tmp/gpg-other   gpg --armor --export "$other" > /out/pub/other.asc
    echo "$fpr" > /out/pub/release.fpr
    echo "$other" > /out/pub/other.fpr
    chmod -R a+rwX /out
'

# Consumer side: a clean container, the repo mounted as a file:// server.
consume() { # $1 = case name, $2 = site dir, $3 = pub key file, $4 = fingerprint, $5 = expect (ok|fail)
    local rc=0
    docker run --rm --network host -v "$2:/site:ro" -v "$OUT/pub:/pub:ro" archlinux:base bash -c '
        set -e
        pacman-key --init >/dev/null 2>&1 && pacman-key --populate archlinux >/dev/null 2>&1
        pacman-key --add /pub/'"$3"' >/dev/null 2>&1
        pacman-key --lsign-key '"$4"' >/dev/null 2>&1
        printf "\n[ds4-translator]\nSigLevel = Required DatabaseRequired\nServer = file:///site\n" >> /etc/pacman.conf
        pacman -Sy --noconfirm ds4-translator
        test -x /usr/bin/ds4-translator
    ' >"$OUT/$1.log" 2>&1 || rc=$?
    if [ "$5" = ok ] && [ "$rc" -eq 0 ]; then echo "ok   $1: installed with signatures enforced"
    elif [ "$5" = fail ] && [ "$rc" -ne 0 ]; then echo "ok   $1: pacman refused it ($(grep -m1 -iE 'error|invalid|signature' "$OUT/$1.log" | cut -c1-90))"
    else echo "FAIL $1 (expected $5, rc=$rc)"; tail -15 "$OUT/$1.log"; FAILED=1; fi
}
FAILED=0
REL="$(cat "$OUT/pub/release.fpr")"

echo "== consume the signed repository"
consume "signed repo, trusted key" "$OUT/site" release.asc "$REL" ok

echo "== negative cases"
cp -r "$OUT/site" "$OUT/tampered"
pkg="$(ls "$OUT"/tampered/ds4-translator-[0-9]*.pkg.tar.zst)"
printf 'x' | dd of="$pkg" bs=1 seek=2000 conv=notrunc status=none
consume "tampered package" "$OUT/tampered" release.asc "$REL" fail

cp -r "$OUT/site" "$OUT/unsigned"
rm -f "$OUT"/unsigned/ds4-translator-[0-9]*.pkg.tar.zst.sig
consume "package without signature" "$OUT/unsigned" release.asc "$REL" fail

consume "repo signed by a key pacman does not trust" "$OUT/site" other.asc "$(cat "$OUT/pub/other.fpr")" fail

if [ "$FAILED" -eq 0 ]; then
    echo "release rehearsal: all checks passed"
else
    echo "release rehearsal: FAILED"
    exit 1
fi
