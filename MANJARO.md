# Package Installer for Manjaro

CachyOS's package installer ([CachyOS/packageinstaller](https://github.com/CachyOS/packageinstaller),
a Qt 6 Widgets fork of MX Linux's mx-packageinstaller, GPL-3.0), built for
Manjaro: its own name and app list, and a few safety changes for a
distribution that moves in batches. Everything else is upstream's, and each
upstream release is merged here.

## What differs from upstream

All of it is in a short stack of commits on top of upstream's history:

| Commit | What |
|---|---|
| Branding through build options | `src/branding.hpp` and `PI_*` CMake variables replace the hard-coded CachyOS name, ids, paths and list URL; upstream's values by default |
| Manjaro's configuration | `cmake/Manjaro.cmake`, and in `manjaro/`: the app list (only packages in Manjaro's repositories), help pages, the desktop file |
| Safety for Manjaro | `src/safety.{hpp,cpp}`: installing waits until the system is up to date (no partial upgrades), no single-package upgrades, pacman's lock waited for, only package names reach pacman, Manjaro's update announcement offered before Upgrade All |
| This file, packaging, the test script | `MANJARO.md`, `packaging/PKGBUILD`, `tools/test-manjaro.sh` |

No upstream file is renamed and no translation file is touched, so merges
only conflict where upstream changes the same lines. Upstream's translated
texts are kept: the distribution's names replace "CachyOS" after
translation (`pi::branded`).

Without `cmake/Manjaro.cmake` the build carries upstream's names and texts,
with these differences: the safety changes are always on, pacman's lock no
longer stops the program from starting, the single-instance key and log
file are named after `PI_APP_ID` (`cachyos-pi-lock`, `cachyos-pi.log`), and
the desktop file name is set for Wayland.

## Build

```bash
sudo pacman -S --needed base-devel cmake ninja rust git qt6-base qt6-tools polkit-qt6
cmake -C cmake/Manjaro.cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
ninja -C build
./build/packageinstaller-manjaro
```

Or as a package: `cd packaging && makepkg -sif` (`-f` builds again after your edits). The first build downloads
fmt, spdlog, cpr, rapidyaml and Corrosion (CPM) and the Rust crates.

## Merging an upstream release

```bash
git remote add upstream https://github.com/CachyOS/packageinstaller.git   # once
git fetch upstream --tags
git merge v1.6.3                    # the new release's tag
manjaro/check-pkglist.sh            # on Manjaro stable: every app still in the repositories
tools/test-manjaro.sh               # build, install and start it in a Manjaro container
```

Conflicts are likeliest in `src/mainwindow.cpp` (the hooks are one or two
lines each), `src/main.cpp` and the install rules at the end of
`CMakeLists.txt`. Upstream's `pkglist.yaml` is not used here, so its edits
never conflict; look at what it added and copy entries that are in
Manjaro's repositories into `manjaro/pkglist.yaml`.

Before releasing, on a real Manjaro machine: install and remove one small
package (pkexec asks for the password), and run Upgrade All once.

## Name

"Package Installer", packaged as `packageinstaller-manjaro`: the Manjaro
name is a registered trademark of Manjaro GmbH & Co. KG, and `manjaro-…`
is how Manjaro names its own packages, so this one does not start with it.
