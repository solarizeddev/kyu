# Kyu Software

Install and remove software on Manjaro and Arch: popular applications by
category, every package in the repositories, and Flatpaks. Qt 6 Widgets, so
it follows your Plasma style and colours. Part of [Kyu](../README.md): its
tray opens it, and "Upgrade All" runs Kyu's update.

Based on CachyOS's package installer (itself a fork of MX Linux's
mx-packageinstaller), GPL-3.0; see UPSTREAM.md for the exact source and
how Kyu's changes are kept apart.

## Build

```bash
sudo pacman -S --needed base-devel cmake ninja rust qt6-base qt6-tools polkit-qt6
cmake -S software -B build-software -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-software
./build-software/kyu-software
```

The first build downloads fmt, spdlog, rapidyaml and Corrosion (CPM) and the
Rust crates.

## The app list

`pkglist.yaml`: categories, each a list of entries; an entry is one or more
package names installed together. Only names in Manjaro's repositories
(checked with `pacman -Si`); it ships with Kyu and is never downloaded.
