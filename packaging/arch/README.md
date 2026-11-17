# Installing ds4-translator from the pacman repository

Every tagged release publishes `ds4-translator` and `ds4-translator-ui` as a
pacman repository, so on Arch Linux (and derivatives such as CachyOS or
Manjaro) you can install and update them with pacman instead of building from
source.

The packages are `x86_64` only. The packages and the repository database are signed with the project's release key (fingerprint `14B1 8F80 91C2 FC4A A6D9  5D3D 38DD 1B46 397B 1836`).

## Add the repository

1. Import the signing key and trust it for this machine. Check that the
   fingerprint pacman prints matches the one above before signing it:

   ```bash
   curl -fsSL https://github.com/Billones142/ds4-translator/releases/download/arch-repo/ds4-translator.pub.asc \
     | sudo pacman-key --add -
   sudo pacman-key --finger 14B18F8091C2FC4AA6D95D3D38DD1B46397B1836
   sudo pacman-key --lsign-key 14B18F8091C2FC4AA6D95D3D38DD1B46397B1836
   ```

2. Append this to `/etc/pacman.conf`, below the official repositories (so a
   same-named package there would win):

   ```ini
   [ds4-translator]
   SigLevel = Required
   Server = https://github.com/Billones142/ds4-translator/releases/download/arch-repo
   ```

3. Sync:

   ```bash
   sudo pacman -Sy
   ```

The same key is also in the repository as
[`ds4-translator.pub.asc`](ds4-translator.pub.asc).

**If a release was published unsigned** (a fork without the signing secret,
see below), use `SigLevel = Optional` instead of `Required` and skip step 1.

## Install

```bash
sudo pacman -S ds4-translator
sudo pacman -S ds4-translator-ui     # optional: Qt settings window and tray applet
sudo systemctl enable --now ds4-translator.service
```

Packages never enable the service on their own, so the last command is
needed once. After that, `sudo pacman -Syu` upgrades it with the rest of the
system, and an upgrade restarts a running daemon.

Check that it works:

```bash
ds4-ctl status
```

## Remove

```bash
sudo pacman -Rns ds4-translator-ui ds4-translator
```

Then delete the `[ds4-translator]` section from `/etc/pacman.conf` if you no
longer want the repository. Removing the package stops and disables the
service.

## Notes

- **Only the latest version is available.** The repository holds one build at
  a time and is replaced by each release, so you cannot downgrade from it. To
  pin an older version, use the copy pacman keeps in `/var/cache/pacman/pkg/`
  (`sudo pacman -U <file>`) or build that tag yourself (`make arch-pkg`).
- **The repository can briefly look inconsistent while a release is being
  published** (a few seconds): pacman may report a 404 for a package file or a
  signature/size mismatch. Run `sudo pacman -Sy` again.
- **The 32-bit spoof shim** is optional. Install `lib32-glibc` if you play
  32-bit games that need it.

## How it is built (for maintainers)

The `arch-repo` job in `.github/workflows/release.yml` runs when a `v*` tag
is pushed, after the `build-and-release` job (the source tarball) has
succeeded:

1. It starts an `archlinux:base-devel` container, enables `[multilib]`, and
   installs the build dependencies.
2. It runs `make arch-pkg arch-repo` as an unprivileged user:
   - `arch-pkg` builds both packages with `makepkg` from `packaging/arch/PKGBUILD.in`;
   - `arch-repo` runs `repo-add` to create the `ds4-translator` database.
3. It uploads the packages and the database to a release tagged `arch-repo`,
   created on the first run, and removes assets left over from earlier builds.
   That release is never marked as "latest", so the one-line installer, which
   reads `releases/latest`, keeps using the versioned releases.

pacman asks for `ds4-translator.db` and `ds4-translator.files`, so those are
uploaded as plain copies: release assets cannot be symlinks, which is what
`repo-add` normally leaves behind.

The dependency list in the workflow mirrors `makedepends` and `depends` in
`packaging/arch/PKGBUILD.in`. Update both together.

Build the same repository locally (as a regular user, on an Arch-based
system):

```bash
make arch-pkg arch-repo
ls build/arch/        # packages, ds4-translator.db, ds4-translator.files
```

To test it before publishing, serve that directory and point a repository
section at it:

```bash
python3 -m http.server -d build/arch 8000
# in /etc/pacman.conf:  Server = http://localhost:8000
```

### Signing

The workflow signs when the repository secret `ARCH_GPG_PRIVATE_KEY` is set,
and publishes unsigned (with a warning in the log) when it is not, so forks
still work. It imports the key into the build user's keyring, passes its
fingerprint as `ARCH_SIGN_KEY`, and `make` then runs `makepkg --sign` and
`repo-add --sign`. The `.sig` files and the public key
(`packaging/arch/ds4-translator.pub.asc`) are uploaded with the packages.

The secret should hold only the **signing subkey** of the release key, never
the primary key:

- the primary key (certify only) stays on your machine, offline; it is the
  one that can extend the expiry, add subkeys and revoke;
- the subkey (sign only) is what GitHub gets. If it leaks, you revoke that one
  subkey and publish a new one without changing the key users trusted.

Create it (the key directory used below is `.signing/`, which is git-ignored):

```bash
export GNUPGHOME=$PWD/.signing/gnupg
gpg --quick-generate-key 'Name (purpose)' ed25519 cert 5y       # primary, certify only
FPR=$(gpg --list-keys --with-colons | awk -F: '/^fpr/ {print $10; exit}')
gpg --quick-add-key "$FPR" ed25519 sign 2y                      # signing subkey
gpg --armor --export "$FPR" > ds4-translator.pub.asc            # public key
gpg --armor --export-secret-subkeys "$FPR" > ci-signing-subkey.secret.asc
```

Then add the secret, in the web UI (Settings → Secrets and variables →
Actions → New repository secret, name `ARCH_GPG_PRIVATE_KEY`, value = the
contents of `ci-signing-subkey.secret.asc`) or with the GitHub CLI:

```bash
gh secret set ARCH_GPG_PRIVATE_KEY < .signing/ci-signing-subkey.secret.asc
```

The subkey in the secret has no passphrase, because the workflow runs
unattended; protect the primary key's passphrase separately.

When the subkey expires, extend it (or create a new one) with the primary key,
re-export the subkeys, and replace the secret and
`packaging/arch/ds4-translator.pub.asc`. Users do not have to do anything:
pacman picks up the new expiry from the refreshed public key only if they
re-import it, so mention it in the release notes.
