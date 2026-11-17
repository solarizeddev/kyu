# What Kyu should be

Written 2026-11-16 with Fable, after looking at what Arch-Update, pamac
(GTK4), Apdatifier, Octopi, Discover, matray and Omarchy get right and wrong,
and checking each claim about Kyu against its code. Sources are at the end.
Out of scope by the owner's decision: an unbootable system after a kernel
update (boot files, initramfs, DKMS).

**Read this first:** Kyu has completed one real update, on the owner's
Manjaro (Plasma 6.7), and real updates in throwaway containers
(`tools/docker/`, 2026-11-17): Manjaro stable, 130 packages behind (news
asked, keyrings first, a systemd restart, three `.pacnew` files), and Arch
from 2026-10-06, 68 behind. Rows that need a real desktop or a rare
failure (a broken keyring, a mirror out of date) are still backed only by
tests against a fake pacman.

## In one sentence

Kyu tells a Plasma user on Manjaro (or Arch) when to update and what to read
first, runs the real pacman in a real terminal, and afterwards says plainly
what is left to do — and nothing else.

## What it should solve (with evidence)

1. **pamac is unreliable for updates on Manjaro.** The GTK4 window disappears
   mid-update while the update keeps running, it crashes when pacman itself is
   updated, and Manjaro's own update announcements list "unreliability of the
   pamac package manager" as a known issue and advise: pacman first, reboot,
   then AUR packages separately. Kyu should do exactly that order.
2. **The right news, on Manjaro.** Arch-Update and informant read Arch's
   news, the wrong feed on Manjaro. (matray shows Manjaro announcements in the
   tray and does nothing else.)
3. **"What now?" after an update.** Restart needed, settings files to merge:
   Arch-Update asks about each in turn; pamac says little.
4. **Failures in plain words.** Other tools show pacman's raw error.
5. **A quiet tray that is right.** Every tool's tracker has tray complaints:
   missing on Plasma, stale counts, notifications that pile up.

Why not just patch Arch-Update with a Manjaro news feed? That would cover
roughly 70% of this list. Kyu differs in three places: a Plasma-native tray
(Arch-Update's is PyQt6), one verdict with one key per thing instead of a
chain of prompts, and failure explanations. If those three don't matter to
you, patching Arch-Update is the cheaper road.

## What it should not be

- A package manager: no search, no install/remove, no list to tick.
- An updater that hides pacman: no GUI transaction, no auto-answered
  "Proceed?", no PackageKit.
- A partial-upgrade risk: never offer or run `pacman -Sy <package>` on its own.
- Something that edits other programs' settings (pamac's autostart included).
- A system rescuer: no boot-file checks, no chroot help (out of scope).
- Configurable in a hundred ways: a handful of settings at most.

## The list

Status: **have** (in Kyu, tested against fakes), **part**, **missing**,
**fix** (in Kyu but wrong), **verify** (unknown). Priority: 1 = needed for
Kyu to be worth using, 2 = clearly better, 3 = nice.

### Checking and the tray

| # | What | Why (who gets it wrong) | Status | Pri |
|---|---|---|---|---|
| 1 | Check without touching pacman's real databases (no `-Sy`), parsed with `LC_ALL=C` | Octopi ran `-Sy`; Apdatifier broke with pacman 7.1; Arch-Update locale bugs | have | 1 |
| 2 | Honour IgnorePkg/IgnoreGroup; honour Flatpak masks | Apdatifier, Octopi | have (pacman); verify (Flatpak: mask a ref, run `kyu-check --json`) | 1 |
| 3 | "Could not check" is its own state, never "up to date" or a stale count | Arch-Update "Cannot fetch Update" on Manjaro | have | 1 |
| 4 | Every check has a timeout and can't block later checks; retry when the network is back after boot or resume | Apdatifier hangs; Arch-Update errors on wake | have: the Flatpak check gives up after 2 min, kyu-check ends after 10 min whatever happens, the timer catches up after sleep, the tray checks after resume and when the network is back after a failed check | 1 |
| 5 | Check again whenever packages change, not only after Kyu's own update (pamac, plain pacman) | Apdatifier keeps the old list | have: the tray watches pacman's package database and Flatpak's change marker and checks once things are quiet | 1 |
| 6 | Tray: theme icons, hidden when up to date, left click opens the card or the update, right click only the menu | Arch-Update's tray on Plasma | have: the card replaces the old panel; the card's states checked in screenshots on Plasma 6.7 (Breeze); the tray icon itself (hidden when up to date, left and right click) not yet confirmed on Plasma | 1 |
| 7 | One tray, not two: print how to turn pamac's tray off (never change it ourselves) | two trays and two notifications on Manjaro | missing | 2 |
| 8 | A failed check is quiet (tooltip "could not check since …"); loud only after about a day | an offline laptop shouldn't show red | missing (today: red at once, like a failed update) | 2 |
| 9 | Held-back packages (IgnorePkg) shown: "N held back" in the tooltip and the verdict | they vanish silently; long holds break Manjaro systems | missing | 2 |
| 10 | Notifications: one at a time, replaced not stacked, expiring; only for new news and a finished update in the background | Arch-Update and Apdatifier pile-ups | part: "Read before updating" (news) and "The update is waiting for you" (seen working on the owner's Plasma); "Updates", "Done", "Failed" not yet | 2 |
| 11 | Icons that don't depend on colour alone | Arch-Update colourblind request | part (tooltip) | 2 |
| 12 | A news fetch that fails says so (Manjaro's forum categories can change) | — | verify | 2 |

### Before pacman

| # | What | Why | Status | Pri |
|---|---|---|---|---|
| 13 | Manjaro's announcement for your branch; Arch news on Arch | Arch-Update/informant: wrong feed on Manjaro | have | 1 |
| 14 | News asked about once per new announcement (remember the last one seen), afterwards one line with the link | every Manjaro update has an announcement; asking every time trains a reflex "y". "Only when it needs doing" can't be read reliably from Manjaro's free text | have: asked once per announcement; opening it from the card counts as read; read ones get one quiet line. On Manjaro each stable update brings a new announcement, so it is still asked once per update; the saving is on repeated runs and when read from the card first | 1 |
| 15 | Wait for pamac or another package manager to finish, then go on | pamac's own background refresh takes the lock routinely | have: waits (up to 10 minutes, Ctrl+C stops), then goes on; a Flatpak-only update ignores pacman's lock | 1 |
| 16 | Keyrings first when they are among the updates: `pacman -Sy --needed archlinux-keyring manjaro-keyring` then `pacman -Su` at once, in the same run | Manjaro advice; the top real failure after long gaps | have: when the last check lists a keyring package (one with files in `/usr/share/pacman/keyrings/`), all installed keyrings go first in their own `pacman -Sy --needed`, then `pacman -Su`, in one shell (sudo asks once); if the new keyring itself is not trusted, or signatures still fail, Kyu says to check the clock and refresh the keys instead of offering the keyring fix again; seen working in a real Manjaro update (container) | 1 |
| 17 | Long gap warning: last full upgrade more than a few weeks ago → "read the announcements in between; keyrings first" | — | missing | 2 |
| 18 | Flatpak-only update without a password | — | have | 2 |

### The update

| # | What | Why | Status | Pri |
|---|---|---|---|---|
| 19 | The real `sudo pacman -Syu` in a real terminal, with its own questions | pamac's daemon/GUI crashes | have: run on the owner's Manjaro and in Arch and Manjaro containers | 1 |
| 20 | Then the AUR helper (yay, paru or `pamac update --aur`) as its own step, if one is installed; counted in the tray too. An AUR failure is reported on its own and never marks the system update as failed | Manjaro's advice: pacman first, AUR after; AUR build failures are pamac's top complaint; Octopi misses AUR updates | have: paru, yay, or pamac with `EnableAUR` and `CheckAURUpdates`; checked against the AUR's web API at most every 6 hours (foreign package names, pacman's version order, held-back ones left out), counted apart; `paru -Sua` / `yay -Sua` / `pamac update --aur` last, after Flatpaks (pamac never without pacman first: it would do the repos too); the system's result is saved before it; its failure is its own line, and a package that failed to build is not offered again until a newer version. `kyu.conf` turns it off or picks the helper. Runs before a restart, not after (Manjaro's "reboot, then AUR" is not followed) | 1 |
| 21 | Then `flatpak update` | — | have | 1 |
| 22 | The terminal you use: Konsole by default, one setting for another | terminal detection is Arch-Update's and Apdatifier's longest bug thread | part (Konsole / xdg-terminal-exec, no setting) | 2 |
| 23 | Closing the window mid-update asks first | pamac's update keeps running hidden | part (the Kyu window asks; plain Konsole asks only Konsole's own question) | 2 |
| 24 | Kept awake while pacman runs | Apdatifier, Omarchy | have | 2 |

### After the update

| # | What | Why | Status | Pri |
|---|---|---|---|---|
| 25 | One verdict line; then only what applies, one key each | Arch-Update asks several questions in turn | have | 1 |
| 26 | Restart needed: kernel (running vs installed), systemd, glibc, NVIDIA; remembered until you restart | Arch-Update false positives (EndeavourOS, kernel-modules-hook) | have (needs real-machine proof) | 1 |
| 27 | Settings files (.pacnew): list them, merge with pacdiff | pamac only lists them in its log | have | 1 |
| 28 | Failures: why, in one sentence; the fix printed, run only on an explicit key | others show raw errors | have | 1 |
| 29 | Failure explanations in any language, or say plainly "English only" | the owner's machine may not be English, which silently turns off 28 | have: pacman stays in your language; its failures are recognised by turning its lines back into English with pacman's own translations (pacman.mo, libalpm.mo). Kyu's own sentences are English, and the README says so. Without a catalog for your language: pacman's last lines, as before | 1 |
| 30 | Mirror failures name Manjaro's own fix (`pacman-mirrors --fasttrack`) | stale mirrors: quiet "up to date" or 404s | have (for the mirror failure); verify (the quiet case) | 2 |
| 31 | Housekeeping as printed lines, never automatic: orphans, package cache, unused Flatpak runtimes | Arch-Update does these with prompts | missing | 3 |
| 32 | A short log of each run | Omarchy keeps one | part (last-run.json) | 3 |

Cut from the draft: restarting system services and firmware updates (scope
creep, Arch-Update parity nobody asked for), Kyu's own translations (until
there is a second user). Branch switching: unsupported; say so in the docs.

### Kyu itself

| # | What | Status | Pri |
|---|---|---|---|
| 33 | One real update on the owner's Manjaro before anything else is built | have (2026-11-17); also real updates in Arch and Manjaro containers | 1 |
| 34 | Rewire the tray's click to the new flow (row 6), then delete the old helper, old panel and `kyu-flatpak` (about 9,000 lines) | have | 1 |
| 35 | Say "Manjaro first; Arch tested less" until tested elsewhere | have (README) | 1 |
| 36 | Survive Plasma, Qt and Konsole updates: small tray; Konsole's part optional | part (the window falls back to Konsole) | 2 |

## Notifications

Researched with Fable from Arch-Update's source and issues, Discover's
history and KDE's guidelines (sources at the end).

**What others do.** Arch-Update sends exactly one: "N updates available"
with a "Run Arch-Update" button, whenever the list of pending updates differs
from the last check, replacing the previous one instead of stacking. Users
still feel nagged (on Arch the list changes at almost every check) and asked
for "only some packages" or "once a day"; those requests were declined. Its
buttons caused two regressions (a blocking `notify-send` left running). It has
nothing for news, finished updates or errors. Discover made "Updates
available" persistent in 2019 and then went back to non-persistent, because
the tray already shows the state; it rate-limits with a timestamp and had a
bug leaving the notification up after updating. KDE's guidelines: give every
notification an urgency, think twice before sending low-importance ones, and
make "must not miss" ones persistent.

**What Kyu should notify.** Through KNotification with a `kyu.notifyrc`, so
each kind can be switched off in System Settings → Notifications (Kyu needs no
setting of its own). One notification per kind, replaced not stacked, closed
when it no longer applies.

| Kind | When | Text and button | Lasts |
|---|---|---|---|
| Updates | only when going from none to some, and at most once a day; never when the list only grows or shrinks | "8 updates: 6 packages, 2 Flatpaks", naming at most the notable ones (kernel, Plasma, Mesa, NVIDIA, systemd); **Update** | goes away by itself; closed when an update starts |
| News | once per new announcement (Manjaro, or Arch news) | "Read before updating" and its title; **Read** (opens it, counts as read); clicking it opens the card | stays until dismissed or read (must not miss) |
| Waiting for you | pacman or sudo is waiting for an answer while the Kyu window is not in front (the terminal part reports silence), plus the taskbar entry asks for attention | "The update is waiting for you"; **Show** | stays until answered |
| Done | the update finished while the Kyu window was not in front | the verdict; **Restart** when a restart is needed | goes away by itself |
| Failed | the update failed while the window was not in front | the one-sentence reason; **Show** | stays until dismissed |
| Could not check | checks have failed for a whole day | "Could not check since yesterday 20:02" | goes away by itself |

Never: every check, "checking", "up to date", a shrinking list.

**Where Fable and I disagree.** Fable: done and failed need no notification,
since they happen in the terminal you are looking at. Me: updates take
minutes, people switch away; "waiting for you" matters most (a "Proceed?" or
a password prompt behind other windows stalls the update without a sign), and
done/failed only fire when the window is not in front. Both: one kind per
event, replaced, and switchable in System Settings.

Status: "News" ("Read before updating") and "Waiting for you" have it (the
latter seen working on Plasma); "Updates",
"Done", "Failed" 2 and "Could not check" 3 (with item 8) are missing.

How "Waiting for you" knows: kyu-update runs pacman, flatpak and the AUR
helper inside util-linux `script` and watches what they print. When the
output stands still for 1.5 s on a question (a "[Y/n]" choice, or a line
ending in "?" or ":"; a password prompt in any language through "[sudo]" or
"password"; paru's and yay's menus by the question above their "==> "; a
pager as "Something to read"; a fingerprint reader as "Your confirmation is
needed"), it writes the question to `~/.local/state/kyu/waiting`, and
removes it once output moves on; its own "Continue?" too. The Kyu window
looks at that file every second and notifies only when it is not in front.

## Compared with what exists

Kyu's column is what it does today, tested only against a fake pacman.

| | Kyu now (untested) | Arch-Update | pamac (GTK4) | Apdatifier | matray |
|---|---|---|---|---|---|
| Checks without `-Sy` | yes | yes | yes (own database copy) | yes | — |
| Right news on Manjaro | yes | no (Arch feed) | no | if you add the feed | yes (only this) |
| Real pacman in a terminal | yes | yes | no (daemon + GUI) | yes | — |
| Closing the window mid-update | Kyu window asks first | same as any terminal | update goes on hidden | same as any terminal | — |
| AUR | with your helper (paru, yay, pamac) | yes | yes | yes | — |
| Flatpak | yes | yes | yes (and Snap) | yes | — |
| Settings files | list + pacdiff | pacdiff | listed in its log | ? | — |
| Restart needed | kernel, systemd, glibc, NVIDIA; remembered | kernel reboot prompt + services | yes (prompt) | optional list | — |
| Housekeeping | no | orphans, cache | yes | no | — |
| Failure explained | yes, in any language pacman is translated to (Kyu's words English) | no | raw error | ? | — |
| Tray on Plasma | theme icons, hidden when idle | PyQt6; some Plasma/Wayland issues | GTK tray | a real Plasma widget | yes |
| Settings | none yet | config file | many | about 110 | few |
| Tested on | stand-ins; screenshots on one Manjaro | many systems | many | many | ? |

Checking without `-Sy` is no longer a differentiator: everyone serious does it.

## Honest bottom line

Kyu's real reasons to exist are items 13, 19, 21, 25–28 on Manjaro:
pamac's unreliability, the right news, and a clear "what now". Two more
(14 and 20) would make it a real pamac replacement but are wrong or missing
today. Everything else matches what Arch-Update already does.

Worth it as the owner's own tool, and possibly for Manjaro Plasma users who
distrust pamac; not worth it as a general Arch updater. Missing or wrong
priority-1 items: 4, 5, 6, 14, 15, 16, 20, 29, 33, 34, 35 — with 33 (one real
update) first.

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
- Notifications: Arch-Update `src/lib/notification.sh`, `src/lib/check.sh`, issues #133, #297, #400, #786, #592, #499, #643, #367, #575;
  Discover: https://commits.kde.org/discover/b9bf6a37ef6f99a3bcf5296bbc67f3820feae5b9, https://bugs.kde.org/show_bug.cgi?id=443351;
  KDE HIG: https://develop.kde.org/hig/status_changes/.
- matray: https://www.github.com/moson-mo/matray; arch-smart-update-checker: https://aur.archlinux.org/packages/arch-smart-update-checker.
- Forum threads on discuss.cachyos.org, forum.endeavouros.com and discuss.kde.org were seen only as search snippets (blocked from this session); Reddit was not searched.
