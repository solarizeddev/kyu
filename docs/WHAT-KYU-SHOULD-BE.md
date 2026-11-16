# What Kyu should be

Written 2026-11-16 with Fable, after looking at what Arch-Update, pamac
(GTK4), Apdatifier, Octopi, Discover, matray and Omarchy get right and wrong.
Sources are listed at the end. Out of scope by the owner's decision: an
unbootable system after a kernel update (boot files, initramfs, DKMS).

## In one sentence

Kyu tells a Plasma user on Manjaro (or Arch) when to update and what to read
first, runs the real pacman in a real terminal, and afterwards says plainly
what is left to do — and nothing else.

## What it should solve (with evidence)

1. **pamac is unreliable for updates on Manjaro.** The GTK4 window disappears
   mid-update while the update keeps running, crashes when pacman itself is
   updated, and Manjaro's own update announcements list "unreliability of the
   pamac package manager" as a known issue and advise: pacman first, reboot,
   then AUR packages separately. Kyu does that order with the real pacman.
2. **The right news, on Manjaro.** Arch-Update and informant read Arch's
   news, which is the wrong feed on Manjaro; Manjaro's announcement thread
   for your branch is what matters there. (matray shows Manjaro announcements
   in the tray but does nothing else.)
3. **"What now?" after an update.** Restart needed, settings files to merge,
   programs on old libraries: today you find out by reading pacman's output
   carefully. Arch-Update asks about each one in turn (several prompts); pamac
   says little.
4. **Failures in plain words.** Keyring, mirror, conflicting files, disk
   space: other tools show pacman's raw error or a generic "something went
   wrong".
5. **A quiet tray that is right.** Users of every tool complain about the
   tray: missing on Plasma, wrong counts, notifications that pile up, icons
   that can't be turned off.

## What it should not be

- A package manager: no search, no install/remove, no package list to tick.
- An updater that hides pacman: no GUI transaction, no auto-answered
  "Proceed?", no PackageKit.
- A system rescuer: no boot-file checks, no chroot help (out of scope).
- Configurable in a hundred ways: Apdatifier has about 110 settings; Kyu
  should need none, and have a handful at most.

## The list

Status: **have** (in Kyu now), **part**, **missing**, **fix** (in Kyu but
wrong). Priority: 1 = needed for Kyu to be worth using, 2 = clearly better,
3 = nice.

### Checking and the tray

| # | What | Why (who gets it wrong) | Status | Pri |
|---|---|---|---|---|
| 1 | Check without touching pacman's real databases (no `-Sy`), with `LC_ALL=C` | Octopi ran `-Sy`; Apdatifier broke with pacman 7.1 | have | 1 |
| 2 | Honour IgnorePkg/IgnoreGroup and Flatpak masks | Apdatifier, Octopi, Arch-Update in non-English locales | have (pacman); verify (Flatpak masks) | 1 |
| 3 | "Could not check" is its own state, never "up to date" or a stale count | Arch-Update "Cannot fetch Update" on Manjaro | have | 1 |
| 4 | Timeouts, cancel, and a retry when the network comes back after boot or resume | Apdatifier hangs; Arch-Update errors on wake | verify | 1 |
| 5 | Check again as soon as an update ends | Apdatifier keeps the old list | have | 1 |
| 6 | Tray: theme icons, hidden when up to date (Passive), left click opens, right click only the menu | Arch-Update's PyQt tray on Plasma | have (built, untested on Plasma) | 1 |
| 7 | One icon, not two: say how to turn pamac's tray off, or do it with consent | two trays and two notifications on Manjaro | missing | 1 |
| 8 | Notifications: one at a time, replaced not stacked, expire; only for news and for a finished update in the background | Arch-Update and Apdatifier pile-ups | missing | 2 |
| 9 | Icons that don't depend on colour alone (tooltip at least) | Arch-Update colourblind request | part (tooltip) | 2 |
| 10 | AUR updates counted (when an AUR helper is installed) | Octopi misses AUR updates | missing | 2 |

### Before pacman

| # | What | Why | Status | Pri |
|---|---|---|---|---|
| 11 | Manjaro's announcement for your branch; Arch news on Arch | Arch-Update/informant: wrong feed on Manjaro | have | 1 |
| 12 | News shown once and not every time: stop at "Continue? [y/N]" only when it says something needs doing, otherwise one line with the link | every Manjaro update has an announcement, so a question every time becomes a reflex "y"; Arch-Update users ask to trim news | **fix** | 1 |
| 13 | Wait for pamac/another package manager to finish, then go on (not stop) | pamac stuck on "Waiting for another package manager" | part (says so, stops) | 2 |
| 14 | Keyring packages first when they are among the updates (`-Sy --needed keyrings && -Su`, no partial upgrade) | Manjaro announcements advise it; keyring failures are common | missing | 2 |
| 15 | Flatpak-only update without a password | — | have | 2 |

### The update

| # | What | Why | Status | Pri |
|---|---|---|---|---|
| 16 | The real `sudo pacman -Syu` in a real terminal, its own questions | pamac's daemon/GUI crashes | have (untested on a real update) | 1 |
| 17 | Then the AUR helper (yay, paru or `pamac update --aur`) as its own step, only if one is installed | Manjaro's advice: pacman first, AUR after | missing | 2 |
| 18 | Then `flatpak update` | — | have | 1 |
| 19 | The terminal you use: Konsole by default, a setting for another | Arch-Update/Apdatifier terminal detection is their longest bug thread | part (Konsole/xdg-terminal-exec, no setting) | 2 |
| 20 | Closing the window asks first; a failed or closed run is detected afterwards | the original goal; pamac keeps running hidden | part (Kyu window asks; plain Konsole doesn't) | 2 |
| 21 | Kept awake while pacman runs | Apdatifier, Omarchy | have | 2 |

### After the update

| # | What | Why | Status | Pri |
|---|---|---|---|---|
| 22 | One verdict line; then only what applies, one key each | Arch-Update asks several questions in turn | have | 1 |
| 23 | Restart needed: kernel (running vs installed), systemd, glibc, NVIDIA; remembered until you restart | Arch-Update false positives (EndeavourOS, kernel-modules-hook) | have (needs real-machine proof) | 1 |
| 24 | Settings files (.pacnew): list them, merge with pacdiff | pamac says nothing | have | 1 |
| 25 | Failures: why, in one sentence, and the fix **printed** (run only on an explicit key) | others show raw errors | have; keep the key opt-in | 1 |
| 26 | Failure explanations in any language (today only when pacman speaks English) | Arch-Update locale bugs | **fix** | 2 |
| 27 | Services to restart (system services on replaced libraries, as root via `checkservices`-style read) | Arch-Update has it | missing | 3 |
| 28 | Orphans and package cache: one line with the command, never automatic | Arch-Update has it | missing | 3 |
| 29 | A short log of each run (what changed, when) | Omarchy keeps one | part (last-run.json only) | 3 |
| 30 | Firmware (fwupd), opt-in | asked for in Arch-Update and Apdatifier | missing | 3 |

### Kyu itself

| # | What | Status | Pri |
|---|---|---|---|
| 31 | Delete the old helper, the old panel and `kyu-flatpak` (about 9,000 lines) | missing | 1 |
| 32 | One real update on the owner's Manjaro before anything else is built | missing | 1 |
| 33 | Say "Manjaro first, Arch tested less" until it is tested elsewhere | missing | 1 |
| 34 | Survive Plasma, Qt and Konsole updates: small tray, Konsole part optional | part (window falls back to Konsole) | 2 |
| 35 | Kyu's own text translatable | missing | 3 |

## Compared with what exists

| | Kyu now | Arch-Update | pamac (GTK4) | Apdatifier | matray |
|---|---|---|---|---|---|
| Right news on Manjaro | yes | no (Arch feed) | no | if you add the feed | yes (only this) |
| Real pacman in a terminal | yes | yes | no (daemon + GUI) | yes | — |
| Survives the UI closing | asks first (Kyu window) | no | update goes on hidden | no | — |
| AUR | no (#17) | yes | yes | yes | — |
| Flatpak | yes | yes | yes | yes | — |
| Settings files | list + pacdiff | pacdiff | no | no | — |
| Restart needed | kernel, systemd, glibc, NVIDIA; remembered | kernel + services | partly | optional list | — |
| Failure explained | yes (English) | no | raw error | no | — |
| Tray on Plasma | theme icons, Passive when idle | PyQt, many Plasma bugs | GTK tray | real plasmoid | yes |
| Settings | none | config file | many | about 110 | few |
| Tested on | one Manjaro machine | many | many | many | ? |

## Honest bottom line

Kyu's real reasons to exist are items 11, 12, 16–18, 22–25 on Manjaro:
pamac's unreliability, the right news, and a clear "what now". Everything
else is matching what Arch-Update already does. Worth it as the owner's own
tool, and possibly for Manjaro Plasma users who distrust pamac; not worth it
as a general Arch updater. Priority 1 items that are missing or wrong: 7, 12,
31, 32, 33.

## Sources

- pamac: [GUI updater often crashes](https://forum.manjaro.org/t/gui-updater-often-crashes/178253),
  [pamac-cli and pamac-manager unreliable](https://forum.manjaro.org/t/pamac-cli-and-pamac-manager-unreliable/184233),
  [crashing while updating](https://forum.manjaro.org/t/pamac-manager-stability-crashing-issues-while-updating/177610),
  [stuck waiting for another package manager](https://forum.manjaro.org/t/update-stuck-at-waiting-for-another-package-manager-to-quit/189026).
- Manjaro advice: [Stable Update 2026-02-01](https://forum.manjaro.org/t/stable-update-2026-02-01-kernels-cosmic-plasma-gnome-mesa/185280/2),
  [2026-05-27](https://forum.manjaro.org/t/stable-update-2026-05-27-kernels-nvidia-firefox-thunderbird-mesa-blender/187938/2),
  [2026-01-04](https://forum.manjaro.org/t/stable-update-2026-01-04-manjaro-26-0-mesa-firefox-libreoffice-cosmic/184517/2).
- Arch-Update issues: https://github.com/Antiz96/arch-update/issues (e.g. #599, #550, #513, #289, #592, #780, #728, #74, #396, #281, #333, #413, #510, #786).
- Apdatifier issues: https://github.com/exequtic/apdatifier/issues (e.g. #70, #149, #156, #161, #179, #230, #237, #165).
- Octopi issues: https://github.com/aarnt/octopi/issues (#134, #306, #296, #434).
- Discover/PackageKit on Arch: https://bugs.kde.org/show_bug.cgi?id=450381, https://bugs.kde.org/show_bug.cgi?id=438129.
- matray: https://www.github.com/moson-mo/matray; arch-smart-update-checker: https://aur.archlinux.org/packages/arch-smart-update-checker.
