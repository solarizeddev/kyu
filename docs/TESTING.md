# Testing Kyu on a real system

Kyu has been built and tested in a container without KDE's libraries, and in
screenshots on one Manjaro machine. This guide covers a run on Manjaro (or
Arch) with Plasma 6: build, automated tests and screenshots (nothing on your
system changes), then a real update.

## 1. Get it and build it

```bash
sudo pacman -S --needed base-devel git cmake ninja qt6-base kstatusnotifieritem kwindowsystem kparts kcoreaddons konsole spectacle flatpak pacman-contrib
```

```bash
git clone -b claude/serene-ritchie-g0q21w https://github.com/solarizeddev/kyu.git
```

```bash
cd kyu
```

## 2. The quick way: one command, one zip

Turn on Do Not Disturb first (each window it opens can post a
notification), then:

```bash
tools/kyu-test-all.sh
```

It takes about 6 minutes:

1. It builds Kyu and runs the automated tests.
2. It runs `kyu-update` against a fake system (fake `sudo`, `pacman` and
   `flatpak`) and keeps the text of every run (`transcripts/`).
3. It opens the card in each state and screenshots it as a window, with
   your window decoration (`cards/`).
4. It opens Kyu's own window (Konsole's terminal, no toolbars) with
   `kyu-update` in it, types the answers itself and screenshots each
   outcome, in your Konsole profile, Konsole's colour schemes, and 60 and 160
   columns wide (`screens/`). Leave the desktop alone meanwhile: a shot is
   taken only while that window is the active one, and the run stops
   otherwise. Plain Konsole windows are used only if Kyu's window was not
   built, or with `KONSOLE=1`; if that Konsole cannot report the active
   window, the run stops at the first shot, and `UNCHECKED=1` takes them
   anyway.
5. It notes the versions of the relevant packages and what `kyu-update`
   reads about your system (the kernel's module folder, the boot image name,
   whether pamac runs, your shell).
6. It writes `~/kyu-test.zip`. Unzip it and look through `kyu-test/screens/`
   (every picture should be a Kyu window, or a Konsole one in the fallback)
   and `kyu-test/cards/`,
   then send it back.

In the text files your home directory, user name and the checkout's path are
replaced with `~`, `<user>` and `<kyu>`. The zip holds no hostname and no copy
of your settings, only names (colour scheme, font, icon theme).

## 3. A real update

This runs a real `sudo pacman -Syu`: do it when you are happy to update.

```bash
build/bin/kyu-check
```

```bash
build/bin/kyu-window
```

(Without Kyu's window built: `konsole -e build/bin/kyu-update`.)

Check that:

- the news shown are Manjaro's announcements for your branch, and "Continue?"
  needs a `y`;
- pacman asks for your password once and asks its own "Proceed?";
- the verdict at the end is right: what changed, a restart if the kernel,
  systemd, glibc or NVIDIA changed, the `.pacnew` files pacman reported;
- `[m]` runs pacdiff, `[r]` opens Plasma's restart dialog, Enter closes;
- closing Kyu's window during the update asks first.

With an AUR helper (paru, yay, or pamac with `EnableAUR` in
`/etc/pamac.conf`) and an AUR package with an update:

- `build/bin/kyu-check` counts it ("AUR 1" in its summary);
- the update runs it last, as `paru -Sua` (or yay's, or `pamac update --aur`),
  and says "· 1 AUR" at the end;
- answering `n` to the helper's question: "AUR packages not updated: … The
  rest is done.", and the tray does not show a failed update.

Then the tray and the card:

```bash
build/bin/kyu-tray &
```

- the icon matches the state (none, updates, news, restart, failed; spinning
  while `kyu-update` runs) on a light and a dark panel;
- a click opens the card; its button does the right thing;
- the tooltip says the same as the card.

### Things that are safe to cause

- **Wrong password three times**, or **`n` at pacman's "Proceed?"**: "Stopped.
  Nothing was installed.", no fix offered.
- **Ctrl+C** at the password prompt or at "Proceed?": Kyu still prints its
  verdict; `/var/lib/pacman/db.lck` is gone afterwards.
- **No network** (`nmcli networking off`): the check fails ("Could not
  check" on the card); an update fails before changing anything, with the
  reason. Turn it back on with `nmcli networking on`.
- **A stale lock** (`sudo touch /var/lib/pacman/db.lck` with no package
  manager running): Kyu says so and offers `[f]` to remove it and update.
- **pacman keeps its lock open while it holds it** (Kyu's `[f]` relies on
  this): run `sudo pacman -Syu`, leave it at its question, and in another
  terminal `sudo ls -l /proc/$(pidof pacman)/fd | grep db.lck` should show the
  lock. Answer `n` afterwards.
- **Another language** (`LANG=de_DE.UTF-8 build/bin/kyu-update` with the
  network off, on a system with that locale): pacman speaks German, and Kyu
  still says "No connection to the mirrors" (in English) with the mirror's
  name. With a language pacman has no translation for: pacman's own last
  lines.

## What to send back

- `~/kyu-test.zip`.
- Build or test failures, with the full output.
- After a real update: the text of Kyu's window (select all, copy) or a
  screenshot, and `~/.local/state/kyu/last-run.json`.
- Screenshots (Spectacle) of the tray icon on a light and a dark panel, and
  of the card.
- Anything that looked wrong, with what you did.
