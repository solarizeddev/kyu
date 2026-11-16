# Kyu

A KDE-native updater for Arch-based systems: a tray icon showing the number of pending repo and Flatpak updates, and (in later milestones) a panel that applies them through a privileged helper. Design and milestones: [`docs/HANDOFF.md`](docs/HANDOFF.md).

Current state: milestone 1, the checker and the tray icon. Kyu does not install anything yet.

## Components

| Path | What it is |
|---|---|
| `src/checker` | `kyu-check`: runs `checkupdates` and libflatpak, writes `$XDG_STATE_HOME/kyu/updates.json`. Never needs root. |
| `src/tray` | `kyu-tray`: StatusNotifierItem with the update count; reads the state file. |
| `src/core` | Shared code: parsers, state file, checks. |
| `data` | `kyu-check.service` / `.timer` (systemd user units), XDG autostart entry for the tray. |

## Dependencies

Runtime: `pacman-contrib` (for `checkupdates`), `flatpak` (libflatpak), Qt 6, KF6 StatusNotifierItem.

Build: CMake ≥ 3.22, a C++20 compiler, Qt 6 (Core, Gui, Widgets, DBus, Test), libflatpak, KF6StatusNotifierItem.

On Arch:

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base kstatusnotifieritem flatpak pacman-contrib
```

## Build and test

```bash
cmake -S . -B build -G Ninja -DKYU_REQUIRE_KSNI=ON
```

```bash
cmake --build build
```

```bash
ctest --test-dir build --output-on-failure
```

Without KF6, drop `-DKYU_REQUIRE_KSNI=ON`; the tray then falls back to `QSystemTrayIcon`. `-DKYU_FLATPAK=OFF` builds without libflatpak (Flatpaks are then reported as not checked).

## Try it from the build tree

```bash
build/bin/kyu-check --json
```

```bash
build/bin/kyu-tray
```

`kyu-check` exits 0 when every source was checked, 2 when one failed (the error is printed and stored in the state file), 1 when the state file could not be written.

Without the installed systemd units, the tray runs `build/bin/kyu-check` itself: one minute after start, then hourly. "Check for updates now" in the tray menu runs it at once. Set `KYU_NO_SYSTEMD=1` to force this mode when the units are installed.

## Install

```bash
sudo cmake --install build
```

This installs both binaries, the systemd user units and `/etc/xdg/autostart/io.github.solarizeddev.kyu.tray.desktop`. At the next login the tray starts and starts `kyu-check.timer`. To check on a timer without the tray:

```bash
systemctl --user enable --now kyu-check.timer
```
