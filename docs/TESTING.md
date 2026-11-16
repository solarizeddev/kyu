# Testing Kyu on a real system

Kyu has so far been built and tested only in a container without KF6. This guide covers the first run on Arch (or Manjaro, CachyOS) with Plasma 6: build, automated tests, a simulated walkthrough of every screen (errors, questions, updates, themes, sizes), then a real install and a real update.

Parts 1 to 3 never touch your system: no root, no packages changed. Part 4 installs Kyu and runs real updates.

## 1. Build

Install the build and runtime dependencies:

```bash
sudo pacman -S --needed base-devel git cmake ninja rust corrosion qt6-base qt6-declarative kirigami qqc2-desktop-style kstatusnotifieritem kwindowsystem flatpak pacman-contrib imagemagick
```

`imagemagick` is optional: it puts each screenshot section on one sheet.

Get the branch:

```bash
git clone -b claude/serene-ritchie-g0q21w https://github.com/solarizeddev/kyu.git
```

```bash
cd kyu
```

Configure and build in `build/`:

```bash
cmake -S . -B build -G Ninja -DKYU_REQUIRE_KSNI=ON
```

```bash
cmake --build build
```

If configuring or compiling fails, send the whole output. The KF6 code (tray, window activation) has never been compiled before this.

## 2. Automated tests

```bash
ctest --test-dir build --output-on-failure
```

Nine suites: the checker, news, pacman output parsing, the state file, the panel logic, the D-Bus client, the helper against a fake pacman, and Flatpak updates against a throwaway local repository. All passed in the container.

## 3. Simulated system: every screen

`tools/kyu-demo.sh` runs the real panel, with your Plasma theme, against a simulated system:

- a test helper on a private session bus;
- a fake pacman that prints what pacman prints;
- a throwaway Flatpak installation with a local repository;
- a fake checker, and a fake `/proc` and kernel modules directory.

Your packages, Flatpaks, pacman database and any running Kyu are not touched. "Restart now" does nothing in this mode.

### Try scenarios by hand

List the scenarios:

```bash
tools/kyu-demo.sh --list
```

Open one, then use the window as you normally would (click, resize, keyboard):

```bash
tools/kyu-demo.sh q-provider
```

Scenarios that run an update start it by themselves. Close the window to end the run.

| Group | Scenarios |
|---|---|
| The list | `list`, `list-long`, `list-empty`, `list-flatpak`, `list-manjaro` |
| Problems before updating | `check-failed`, `stale-lock`, `interrupted` |
| Updating | `running`, `mirror-changed`, `nothing`, `finished` |
| pacman's questions | `q-provider`, `q-conflict`, `q-replace`, `q-key`, `q-skip`, `q-unknown`, `cancelled`, `answered` |
| Failures | `fail-network`, `fail-mirror`, `fail-keyring`, `fail-files`, `fail-disk`, `fail-deps` |
| Pages after an update | `page-output`, `page-changes`, `page-configs`, `page-diff`, `page-processes` |

Theme, fonts, scale and size apply to this panel only, through a private copy of your `kdeglobals`. Your desktop does not change.

```bash
SCHEME=BreezeDark tools/kyu-demo.sh finished
```

```bash
FONT_PT=16 SCALE=1.5 tools/kyu-demo.sh list-long
```

```bash
SIZE=300x420 tools/kyu-demo.sh q-conflict
```

`SCHEME` takes any installed colour scheme name (`ls /usr/share/color-schemes ~/.local/share/color-schemes`) or a path to a `.colors` file.

Things to try by hand:

- **Keyboard:**
  - Enter updates.
  - 1–9 pick an answer.
  - `l` opens the output, and Esc goes back.
  - On a question, `l` then Enter must not answer: the question stays.
  - Tab to "Cancel the update", then Enter, must cancel.
- **Resizing:** drag the window very small and very wide.
- **Tooltips:** hover a cut-off package row; the tooltip shows the full text.
- **Errors:** click a long error to expand it.
- **Copying:** use "Copy" next to a fix command.

### Screenshot everything

```bash
tools/kyu-screens.sh ~/kyu-screens
```

Windows open and close by themselves for about five minutes. Leave the desktop alone while they do. The script takes these sections:

| Section | What |
|---|---|
| A | every scenario, your theme, default size |
| B | colour schemes (default: BreezeLight, BreezeDark) × list, question, finished, failure |
| C | window sizes 300×420 to 1920×1080 × long list, question, finished |
| D | fonts 8, 12, 16, 20 pt × long list, finished |
| E | scale 1.25, 1.5, 2 × long list, finished |

To include every installed scheme, or only some sections:

```bash
SCHEMES=all tools/kyu-screens.sh ~/kyu-screens
```

```bash
SECTIONS="B D" SCHEMES="BreezeLight BreezeDark BreezeClassic" tools/kyu-screens.sh ~/kyu-screens
```

With ImageMagick installed, `~/kyu-screens/sheet-A.png` and the other sheets hold one section each. A scenario that failed leaves a `.log` next to its name.

`OFFSCREEN=1` renders without windows. It is quicker, but it skips the compositor and may load fonts and icons differently, so prefer the default for screenshots you send.

## 4. Real install and real updates

### Install

Build a package from this checkout and install it. It installs to `/usr`, the only place the D-Bus policy and polkit action work.

```bash
cd packaging/arch
```

```bash
makepkg -si
```

Add `--nocheck` if the tests already ran in part 2. To remove Kyu later:

```bash
sudo pacman -R kyu-git
```

### Check each part

Make the system bus read the new policy (Arch usually does this on install):

```bash
sudo systemctl reload dbus
```

The checker, without root:

```bash
kyu-check
```

Compare its list with `checkupdates`. They should match.

The helper, started by D-Bus on demand:

```bash
kyu-update --state
```

The tray starts at login. Log out and in, or start it now:

```bash
kyu-tray &
```

The periodic check:

```bash
systemctl --user list-timers kyu-check.timer
```

### A real update

First from a terminal, to compare with what you know. It runs `pacman -Syu` through the helper and asks for your password through polkit:

```bash
kyu-update
```

Then later, when updates are pending, from the tray: click the icon, then "Update all". Check that:

- the panel's list matches `checkupdates`, grouped by repository;
- polkit asks for your password once;
- progress ticks off packages, and the log tail moves;
- closing the panel mid-update does not stop it, and clicking the tray icon again reattaches;
- the result names a restart if the kernel changed, and lists any `.pacnew` files;
- the log is in `/var/log/kyu/`.

### Real failures that are safe to cause

- **No network:**
  1. Turn networking off (`nmcli networking off`).
  2. Click "Check again". The list should keep the last result and say the check failed.
  3. Click "Update all". pacman should fail while syncing, with nothing changed.
  4. Turn networking back on (`nmcli networking on`).
- **Lock held:**
  1. In a terminal, run `sudo pacman -Syu` and leave it at its prompt.
  2. Click "Update all". It should be refused, naming the pacman process.
  3. Answer `n` in the terminal.
- **Stale lock:**
  1. With no pacman running, run `sudo touch /var/lib/pacman/db.lck`.
  2. Open the panel. It should offer "Remove the stale lock".

### Tray icon

Plasma colours the icon for the panel it sits in. Check it on a light panel and on a dark one, for example with the Breeze Twilight global theme. Check the count overlay and the tooltip. Also open the panel, cover it with another window, then click the tray icon: the panel should come to the front with focus (on Wayland this needs `kwindowsystem`).

## What to send back

- Build or test failures, with the full output.
- `~/kyu-screens/sheet-*.png`, or the whole folder, plus any `.log` files in it.
- Screenshots (Spectacle) of the real panel and the tray icon on a light panel and a dark one.
- Anything that looked wrong, with the scenario name or what you did.
- After a real update: the log from `/var/log/kyu/`, and the output of `kyu-update` if you used it.
