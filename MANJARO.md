# Package Installer for Manjaro

CachyOS's package installer ([CachyOS/packageinstaller](https://github.com/CachyOS/packageinstaller),
GPL-3.0), version 2.0 (the QML app store: Discover, Browse, Installed,
Updates, AUR with PKGBUILD review, Flatpak, AppImage, colour themes), built
for Manjaro: Manjaro's name and logo, Manjaro's app list and mirror ranking.
Everything else is upstream's, and each upstream release is merged here.

This tracks upstream's `fresh` branch until 2.0.0 is tagged, then its
releases.

## What differs from upstream

| Where | What |
|---|---|
| `src/brand.{hpp,cpp}` (new), two lines in `src/main.cpp` | With a distribution's name set: a translator that puts "Manjaro" in place of "CachyOS" (and rewrites the few texts that say more than the name); the app's pictures of the CachyOS logo replaced by the logo from the icon theme (`manjarolinux`, from `manjaro-icons`); window icon, desktop file name and settings name |
| `src/alpmdb.cpp` | Which repositories are shown as the distribution's own: all of them on Manjaro (upstream: those starting with `cachyos`, the rest show as "Arch") |
| `src/helper/shared.hpp`, `src/helper/main.cpp` | The mirror ranking program: `pacman-mirrors --no-color --fasttrack 10` |
| `src/transaction.cpp` | An uninstalled build finds its helper by the installed name (one line) |
| `CMakeLists.txt` | The `PI_*` options; the program, scanner and helper names; the polkit file with its own ids; the desktop file; no CachyOS icon or AppStream entry |
| `cmake/Manjaro.cmake`, `manjaro/` | The Manjaro settings, the app list (only packages in Manjaro's repositories), its check, the desktop file |
| `MANJARO.md`, `packaging/PKGBUILD`, `tools/test-manjaro.sh` | This file, the package, the container test |

No QML file and no upstream text is edited, so a merge only conflicts where
upstream changes the few lines above. Without `cmake/Manjaro.cmake` the
build is upstream's; the container test builds and tests both.

Known: every sync repository is badged "Manjaro", third-party ones (such
as chaotic-aur) too; the helper's own error messages still start with
`cachyos-pi-helper:`; upstream's helper test checks upstream's mirror
command, the container test checks ours.

Not yet done for Manjaro (next step):
- Upstream's update notice is CachyOS's and stays off here; Manjaro's
  stable-update announcement should be shown before Update all.
- Installing a package does not check that the system is up to date, and
  pacman's lock is not waited for.

## Build

```bash
sudo pacman -S --needed base-devel cmake ninja git qt6-tools appstream-qt archlinux-appstream-data \
    elfutils kimageformats libjxl manjaro-icons pacman-mirrors qt6-declarative qt6-svg
cmake --fresh -C cmake/Manjaro.cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build && ctest --test-dir build
./build/packageinstaller-manjaro
```

Run from the build directory, package operations use the helper next to
it behind polkit's generic prompt, and the taskbar shows a generic icon
(Wayland takes it from the installed desktop file). `--fresh` matters when
`build/` holds an older configuration: `-C` does not override a cache. Or as a package: `cd packaging && makepkg -sif`
(`-f` builds again after your edits). The first build downloads rapidyaml
and backward-cpp (CPM).

AUR builds need `paru`, which Manjaro does not package: build it from the
AUR once. AUR packages are written for Arch, which is often ahead of
Manjaro stable; the settings page says so.

## Merging an upstream release

```bash
git remote add upstream https://github.com/CachyOS/packageinstaller.git   # once
git fetch upstream --tags
git merge v2.0.0                    # the new release's tag (until then: upstream/fresh)
manjaro/check-pkglist.sh            # on Manjaro stable: every app still in the repositories
tools/test-manjaro.sh               # build both ways, test, install and start it in a Manjaro container
```

After a merge, look for new texts that name CachyOS or its tools:

```bash
git grep -n -i 'cachy' -- qml src | grep -i 'qsTr\|tr("'
```

Most only need the name changed, which happens by itself. A text that says
more (CachyOS's repositories, mirrors, tools) gets a whole-text entry in
`src/brand.cpp`. Upstream's `pkglist.yaml` is not used here; copy new
entries that are in Manjaro's repositories into `manjaro/pkglist.yaml`.

Before releasing, on a real Manjaro machine: install and remove one small
package (pkexec asks for the password), rank the mirrors, and run Update
all once.

## Name and logo

Shown as "Manjaro / Package Installer", window title "Package Installer for
Manjaro", packaged as `packageinstaller-manjaro`. The Manjaro name and logo
are trademarks of Manjaro GmbH & Co. KG. The logo is not copied into this
repository: it is read from the icon theme (`manjaro-icons`), and
`manjaro-…` is how Manjaro names its own packages, so this one does not
start with it.
