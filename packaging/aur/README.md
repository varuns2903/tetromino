# Arch Linux (AUR) packages

Two ready-to-publish packages:

| Package | What it installs | Builds from |
|---|---|---|
| [`tetromino`](tetromino/PKGBUILD) | `/usr/bin/tetromino`, man page, docs | the release's source tarball (runs the unit tests) |
| [`tetromino-bin`](tetromino-bin/PKGBUILD) | the same, prebuilt | the release's static binary (x86_64, aarch64) |

Both were built, linted with `namcap`, installed and run in a clean Arch
container. They aren't on the AUR yet.

## Install without the AUR

```bash
git clone https://github.com/varuns2903/tetromino.git
cd tetromino/packaging/aur/tetromino      # or tetromino-bin
makepkg -si
```

## Publishing to the AUR (maintainer)

Needs an AUR account with an SSH key
(<https://aur.archlinux.org/register>). For each package, once:

```bash
git clone ssh://aur@aur.archlinux.org/tetromino.git aur-tetromino
cp packaging/aur/tetromino/{PKGBUILD,.SRCINFO} aur-tetromino/
cd aur-tetromino
git add PKGBUILD .SRCINFO
git commit -m "Initial import: tetromino 1.0.1"
git push
```

(and the same with `tetromino-bin`). Cloning a name that doesn't exist yet
creates it on the first push.

## Updating for a new release

After the release workflow has published `vX.Y.Z`:

```bash
cd packaging/aur/tetromino        # then tetromino-bin
sed -i 's/^pkgver=.*/pkgver=X.Y.Z/; s/^pkgrel=.*/pkgrel=1/' PKGBUILD
updpkgsums                        # from pacman-contrib: refreshes sha256sums
makepkg -f && namcap PKGBUILD *.pkg.tar.zst
makepkg --printsrcinfo > .SRCINFO
```

Commit the result here and push the same two files to the AUR repository.
