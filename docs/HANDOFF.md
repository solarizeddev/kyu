# Kyu: design handoff

Working name: **Kyu** (KDE "K" + pacman `-Syu`, read as "queue"). Check the name on GitHub, the AUR and KDE's app list before committing to it.

This file records the decisions from the design session so a coding session can start without the chat history. Static copies of the sketches are in `sketches/`.

## What it is

A KDE-native **updater** for Arch-based systems (Manjaro, Arch, CachyOS, EndeavourOS). A tray icon plus one panel that lists pending updates and applies them. It is not a package manager or app store: no search, browse or install in the first version.

## Why

- pamac (Manjaro's tool) crashes mid-transaction, and Manjaro's own update notes steer users to pacman or Octopi.
- Discover should not manage pacman packages on Arch (PackageKit backend is marked "not recommended").
- Octopi works but has no Flatpak support and looks dated. Arch-Update has a tray icon but updates in a terminal.
- Gap: one tray icon and one update flow covering repo packages and Flatpaks, native to Plasma, that survives a UI crash.

Prior art worth reading before coding: `kalu` (same GUI + privileged D-Bus helper split, since 2012; its libalpm binding broke on pacman releases), `arch-update`, `shelly` (CachyOS default), Octopi's notifier.

## Architecture

```
User session (no root)
  update checker  ->  tray icon  ->  update panel
                                        |        \
                                        |         libflatpak (no helper needed)
                                        v
                          privileged helper (D-Bus, polkit, root)
                              |                     |
                           pacman            transaction log
```

- **Update checker:** user-level timer. Uses `checkupdates` (no root, no partial sync) and counts Flatpak updates.
- **Tray icon:** StatusNotifierItem with a count. Opens the panel.
- **Update panel:** a viewer only. It can close or crash without affecting a running update, and reattaches by reading the log.
- **Privileged helper:** the only code that runs as root. Owns the transaction from start to end, streams progress over D-Bus, writes the transaction log.
- **Flatpak:** updated from the user session through libflatpak, shown in the same list.

Decisions:
- **Drive the `pacman` binary and parse its output** at first. Do not link libalpm yet (kalu's history shows the maintenance cost).
- **AUR is out of scope for v1.** If added later it is a separate, explicit step, never part of the system upgrade.
- **Suggested stack:** Qt6, Kirigami/QML, KStatusNotifierItem, polkit (KAuth or plain), helper in C++ or Rust.

## UI rules

1. **List first.** The first screen is the package list with real names and `old -> new` versions.
2. **Group only by repo** (`core`, `extra`, `flathub`, ...). No hand-maintained categories of any kind.
3. **Nothing hidden, nothing renamed.** Show the exact commands that run. Quote pacman's own messages on prompts.
4. **No unsafe options.** No per-package skip for repo packages (partial upgrades break Arch). "Hold a package" maps to `IgnorePkg` and carries a warning.
5. **Facts over reassurance.** Copy states what happened ("12 updated, restart required"), not how to feel about it.
6. **Never hardcode a colour.** Use `Kirigami.Theme` roles and theme icon names so the app follows the user's colour scheme, accent, fonts and icons. The hex values in the sketches are stand-ins for those roles.
7. **Derived, not curated.** Restart flag = running kernel differs from installed kernel, or updated core libraries still in use. Descriptions come from package metadata.

## Screens (see `sketches/`)

| File | Screen | Notes |
|---|---|---|
| `ready-*.html` | Updates list | Count, total size, restart flag. Rows: name, old -> new, grouped by repo. "Update all" plus a menu: update at shutdown, hold a package, copy as command. |
| `details-*.html` | Package info | Versions, download size, size change, required-by, install reason, changelog, files. "Hold this package" with IgnorePkg warning. |
| `running-*.html` | Updating | Percentage, thin bar, the same list ticking off per package. "Closing this panel will not stop the update." |
| `log-*.html` | Pacman output | Live output, commands shown, helper pid and privilege info, copy log, follow in Konsole. |
| `ask-*.html` | Question | Provider choice as radios with a suggested default, pacman's message quoted. Same pattern for conflicts, replacements, key imports. |
| `finished-*.html` | Done | "N updated, restart required", running vs installed kernel, restart now or later, .pacnew review, list of what changed, roll back. |

All numbers, versions and package names in the sketches are sample data.

Colour roles used in the sketches and their Kirigami equivalents:

| Sketch token | Kirigami.Theme role |
|---|---|
| `win` | `backgroundColor` (Window set) |
| `tile` | `backgroundColor` (View set) |
| `head` | `alternateBackgroundColor` |
| `text` | `textColor` |
| `muted` | `disabledTextColor` |
| `accent` | `highlightColor` |
| `ok` | `positiveTextColor` |
| `warn*` | `neutralTextColor` / `neutralBackgroundColor` |
| `link` | `linkColor` |

## Milestones

1. **Checker + tray icon.** Timer, `checkupdates`, Flatpak count, icon with total.
2. **Helper, repo updates only.** D-Bus service behind polkit that runs `pacman -Syu`, streams progress, writes the log. Test by killing the UI mid-update.
3. **Update panel.** List, "Update all", live progress, pacman output view.
4. **Prompts.** Providers, conflicts, replaced packages, keyring imports, relayed helper -> panel. Hardest part.
5. **Flatpak in the same list.**
6. **After-update care.** Restart detection, .pacnew files, distro news before big updates.
7. **Failure states** (not sketched yet): out-of-sync mirror, keyring error, interrupted update and reattach, no network, lock file present.
8. **Later, optional:** upgrade simulation (dry run) before applying, snapshot before update and roll back (Btrfs/Snapper or Timeshift), watched packages, AUR as a separate step, settings.

## Open questions

- Helper language: C++ (KAuth integration) or Rust.
- Panel form: Plasma applet (plasmoid) or a small standalone window opened from the tray. A plasmoid is more native but ties releases to Plasma versions.
- Manjaro specifics: branches, `pacman-mirrors`, and whether `checkupdates` behaves on Manjaro mirrors.
- How to answer pacman's interactive prompts reliably when driving the binary (pty and output parsing vs a `--print`/dry-run pre-pass to detect questions up front).
- Whether the roll back feature is in scope at all.
