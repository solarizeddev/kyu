# Kyu

A Plasma update notifier for Manjaro (and Arch). Kyu tells you when to
update and what to read first, runs the real `sudo pacman -Syu` and then
`flatpak update` in a terminal, and afterwards says plainly what is left to
do: restart, merge a settings file, or why it failed and how to fix it.

What Kyu should be, what it should solve and what is still missing:
[`docs/WHAT-KYU-SHOULD-BE.md`](docs/WHAT-KYU-SHOULD-BE.md). Why the update
runs in a terminal: [`docs/TERMINAL-FIRST.md`](docs/TERMINAL-FIRST.md).

**Status:** tested against a fake pacman and in screenshots on one Manjaro
machine; it has not yet completed a real update. Manjaro first; Arch is
tested less.

## Components

| Program | What it does |
|---|---|
| `kyu-tray` | The tray icon: your icon theme's update icons (none, updates, news to read first, restart to finish, failed) and a spinning one while updating. Hidden when there is nothing to do. Left click opens the card. |
| `kyu` | The card: what is going on and the one thing to do next (Update, Restart, Merge…, Check for updates). |
| `kyu-window` | Kyu's own window with Konsole's terminal inside (your window decoration, no toolbars), running `kyu-update`. Built when KF6 Parts is available; otherwise the update opens in Konsole. |
| `kyu-update` | The update itself, in any terminal: the news since your last update (Manjaro's announcements on Manjaro), `sudo pacman -Syu`, `flatpak update`, then one verdict and one key per thing left to do. `kyu-update --merge` only merges settings files (pacdiff). |
| `kyu-check` | Counts pending updates without touching pacman's databases (like `checkupdates`), plus Flatpaks and news; run by a systemd user timer. |

Nothing runs as root except pacman itself (through `sudo`, `doas` or
`pkexec`) and what you start with a key.

## Build

```bash
sudo pacman -S --needed base-devel git cmake ninja qt6-base kstatusnotifieritem kwindowsystem kparts kcoreaddons flatpak pacman-contrib
```

```bash
cmake -S . -B build -G Ninja -DKYU_REQUIRE_KSNI=ON -DKYU_REQUIRE_WINDOW=ON
```

```bash
cmake --build build
```

```bash
ctest --test-dir build --output-on-failure
```

## Try it from the build tree

The card in each state, without touching anything:

```bash
build/bin/kyu --demo updates
```

(States: `updates`, `flatpaks`, `uptodate`, `checking`, `checkfailed`,
`updating`, `restart`, `settings`, `failed`.)

The real thing: check, then the tray, then the update (a real
`sudo pacman -Syu`):

```bash
build/bin/kyu-check
```

```bash
build/bin/kyu-tray &
```

```bash
build/bin/kyu-window
```

## Test on a real system

[`docs/TESTING.md`](docs/TESTING.md): one command builds, tests and
screenshots everything into a zip, without changing your system.

## Install

`packaging/arch/PKGBUILD` builds a package from this checkout:

```bash
cd packaging/arch && makepkg -sif
```

The tray starts at login (XDG autostart); the check runs from the
`kyu-check.timer` user unit:

```bash
systemctl --user enable --now kyu-check.timer
```
