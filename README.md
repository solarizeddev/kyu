# Kyu

A Plasma update notifier for Manjaro (and Arch). Kyu tells you when to
update and what to read first, runs the real `sudo pacman -Syu`, then
`flatpak update` and your AUR helper, in a terminal, and afterwards says plainly what is left to
do: restart, merge a configuration file, or why it failed and how to fix it.

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
| `kyu-update` | The update itself, in any terminal: the news since your last update (Manjaro's announcements on Manjaro), `sudo pacman -Syu` (a new keyring first, on its own), `flatpak update`, then your AUR helper (paru, yay, or pamac with the AUR on), then one verdict and one key per thing left to do. `kyu-update --merge` only merges configuration files (pacdiff). |
| `kyu-check` | Counts pending updates with `checkupdates` (pacman's real databases are not touched), plus Flatpaks, news, and AUR packages from the AUR's web API when an AUR helper is installed; run by a systemd user timer. |

Nothing runs as root except pacman itself (through `sudo`, `doas` or
`pkexec`) and what you start with a key.

Kyu's own words are English. pacman keeps your language: Kyu recognises
its failures with pacman's own translations, where they have the message
(pacman leaves some messages in English in some languages; those are
recognised as they are). Otherwise Kyu shows pacman's own last lines.

## Build

```bash
sudo pacman -S --needed base-devel git cmake ninja qt6-base kstatusnotifieritem kwindowsystem kparts kcoreaddons kiconthemes konsole flatpak pacman-contrib
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

## The AUR

With paru, yay, or pamac (with "EnableAUR" and "CheckAURUpdates" on in
`/etc/pamac.conf`) installed, Kyu counts AUR updates and runs the helper's
AUR update last. To check, Kyu sends the names of your packages from outside
the repos (`pacman -Qm`) to aur.archlinux.org, at most every 6 hours, as the
helpers do when they check. Packages pacman holds back (`IgnorePkg`,
`IgnoreGroup`) are left out; development packages (`-git`) are not checked
for new commits. A package whose build failed is not offered again until the
AUR has a newer version.

To leave the AUR alone, in `~/.config/kyu/kyu.conf`:

```ini
[AUR]
check=false
```

Or, to pick the helper when more than one is installed:

```ini
[AUR]
helper=yay
```

"Check for updates" on the card asks the AUR right away.

## Install

`packaging/arch/PKGBUILD` builds a package from this checkout:

```bash
cd packaging/arch && makepkg -sif
```

The tray starts at login (XDG autostart) and starts the `kyu-check.timer`
user unit itself. Without the tray, enable the timer by hand:

```bash
systemctl --user enable --now kyu-check.timer
```

Coming from an older Kyu (with its root helper): its logs in `/var/log/kyu`
are no longer used; remove them if you like:

```bash
sudo rm -r /var/log/kyu
```
