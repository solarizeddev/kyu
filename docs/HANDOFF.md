# Kyu: design handoff

Working name: **Kyu** (KDE "K" + pacman `-Syu`, read as "queue"). Check the name on GitHub, the AUR and KDE's app list before committing to it.

This file records the decisions from the design session so a coding session can start without the chat history. Static copies of the sketches are in `sketches/` (also packed in `kyu-handoff.zip`).

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
- **Stack:** checker, tray and panel in C++ with Qt6, Kirigami/QML and KStatusNotifierItem. Helper in **Rust** (zbus, polkit called directly over D-Bus). Decided 2026-11-16, see below.
- **Panel form:** a small standalone window opened from the tray, not a plasmoid, so releases are not tied to Plasma versions.
- **No roll back.** Kyu does not take or restore snapshots (decided 2026-11-16: unnecessary complexity). Users who run snap-pac or similar keep their snapshots through pacman hooks, outside Kyu.

### Why Rust for the helper, and not KAuth

KAuth suits short one-shot actions with one-way progress. The helper needs more than that: it must ask the panel questions mid-transaction (milestone 4) and let a new panel reattach to a running update. Neither fits KAuth. So either language needs a custom system-bus service that calls polkit itself, which removes C++'s main advantage. The UI/helper contract is language-neutral anyway: a D-Bus introspection XML (qdbusxml2cpp on the Qt side, zbus-xmlgen on the helper side) plus a versioned log format. Rust then gives exhaustive matching for the prompt/transaction state machine and memory safety for root code that parses text. Build the helper through Corrosion so `cmake --build` stays the only entry point. Main risk: two toolchains and a small pool of Rust maintainers; keep the helper small and its dependencies few and locked.

### Helper requirements (from the language review, for milestones 2 and 4)

- **Reattach needs live state, not only the log.** Add a D-Bus `GetState` (phase, pending prompt, transaction id). Keep the log for history.
- **Prompt ownership.** Only the uid that started the transaction may answer or reattach. Define what happens when a prompt is pending and no panel is open: wait, or time out and abort.
- **Fixed operations only.** The D-Bus API never takes client-supplied arguments, package lists or environment.
- **Lifetime.** Do not kill pacman on SIGHUP or service stop mid-transaction. Hold a logind inhibitor against shutdown and sleep while running. Handle a stale `db.lck`.
- **Show the real transaction.** The `checkupdates` list and the actual `-Syu` set can differ (mirror changed in between). Report the real set before committing.
- **Locale.** Parsing needs `LC_ALL=C`, which conflicts with UI rule 3 (quote pacman's messages): the quotes would be in English. Decide per message, or parse with C and show localized text separately.
- **Prompts are not a stable interface.** A `--print` pre-pass will not surface every question. Pin tested pacman versions and fail closed on unknown prompts, or reconsider libalpm's question callback just for prompts.
- **"Update at shutdown"** (in the Updates list menu) has no mechanism yet. It needs something like systemd offline updates, which is a separate design.

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
| `log-*.html` | Pacman output | Live output, commands shown, helper pid and privilege info, copy log, follow in Konsole. (Ignore the sketch's "snapshot taken" line; no snapshots.) |
| `ask-*.html` | Question | Provider choice as radios with a suggested default, pacman's message quoted. Same pattern for conflicts, replacements, key imports. |
| `finished-*.html` | Done | "N updated, restart required", running vs installed kernel, restart now or later, .pacnew review, list of what changed. (The sketch still shows a roll back row; it is out of scope.) |

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
8. **Later, optional:** upgrade simulation (dry run) before applying, watched packages, AUR as a separate step, settings.

## Open questions

- Manjaro specifics: branches, `pacman-mirrors`, and whether `checkupdates` behaves on Manjaro mirrors.
- How to answer pacman's interactive prompts reliably when driving the binary (pty and output parsing vs a `--print`/dry-run pre-pass to detect questions up front). See "Helper requirements" above.
- Order of pacman and Flatpak in one run: the log sketch runs Flatpak first, the running sketch runs it after pacman.

## Status

**Milestone 1 (checker + tray icon): done.** See the README for build and run steps.

- `kyu-check` runs `checkupdates` against a private database copy (`$XDG_CACHE_HOME/kyu/checkup-db`), looks up each package's repo in that copy with `pacman -Sl`, and counts Flatpak updates in all system installations plus the user installation through libflatpak. It writes `$XDG_STATE_HOME/kyu/updates.json` atomically. A source that fails keeps its last good list and records the tool's own error message.
- `kyu-check.timer` (systemd user unit) runs it hourly. `kyu-tray` starts the timer at login. Without a systemd user manager, the tray runs `kyu-check` itself every hour.
- `kyu-tray` watches the state directory and shows the theme icon `update-low` with the count drawn in the palette's highlight colours, a tooltip with counts per repo and remote, and the check time or failure. The icon goes Passive (hidden in the tray's overflow) when there is nothing to do. The menu has "Check for updates now".

**Milestone 2 (helper, repo updates only): done.** Protocol: `docs/helper-protocol.md`.

- `kyu-helper` (Rust, zbus, polkit over D-Bus) is D-Bus activated on the system bus as `io.github.solarizeddev.kyu.Helper`. It has two methods, `StartUpdate` and `GetState`, and one signal, `Event`, carrying JSON records. Each record is also a line of `/var/log/kyu/<tx>.jsonl`.
- It runs `pacman -Syu --noprogressbar --color never` with `LC_ALL=C` and a cleared environment. It answers only "Proceed with installation?". Any other question stops pacman with SIGINT before it changes anything, and the summary says so.
- It refuses to start when `db.lck` exists. It holds a logind inhibitor during the update, ignores SIGHUP, and on SIGTERM waits for pacman to finish. It exits after 5 idle minutes. After a restart, `GetState` reports the previous transaction, including "interrupted" when its log has no result.
- `kyu-update` (Qt, `HelperClient`) starts an update or reattaches with `--attach`. Records come from the log first, then from signals, ordered by sequence number, with no gaps or duplicates. It reports when the helper disappears mid-update.
- Tested with a fake pacman on a private session bus (`ctest`): killing the client mid-update and reattaching, busy, locked, an unanswerable question, pacman failure, SIGTERM mid-update, and helper killed mid-update. Separately checked on a private system bus with the real polkitd: the bus policy lets only root own the name, the polkit action loads, a user without an agent gets `NotAuthorized`, and root is authorized.
- Not yet run against real pacman (the development container is Ubuntu). First thing to do on an Arch machine: install, `sudo systemctl reload dbus`, run `kyu-update`, and compare its output and log with a manual `pacman -Syu`.

**Milestone 3 (update panel): done.** Screenshots in `docs/screenshots/`, made with `tools/preview-panel.sh`.

- `kyu` is a standalone Qt Quick window using Kirigami for theme roles, units, icons and headings, and stock Qt Quick Controls for the rest (Plasma's `org.kde.desktop` style when installed). It is single-instance over the session bus. Clicking the tray icon, or "Show updates" in its menu, starts it.
- **List:** count, total package size, check time, check failures quoted; packages grouped by repo in pacman.conf order (kyu-check now sorts them that way and records each package's size from `pacman -Sup --print-format`), Flatpaks under their remote. "Update all", plus a menu with "Copy as command" (`sudo pacman -Syu`) and "Check again". Flatpaks are listed with a note that "Update all" does not update them yet (milestone 5).
- **Updating:** completed share of package operations, the stage or "upgrading N of M", the same list ticking off (done, working, waiting; packages pacman touched that the check did not list appear under "not in the last check"). "Closing this panel will not stop the update."
- **pacman output:** live output with Kyu's own lines (the command, the answered question) marked, pacman pid, root via polkit, the log path; "Copy log" and "Follow in Konsole" (runs `kyu-update --attach`).
- **Finished:** pacman's summary as the title, time, duration and exit code; on failure, pacman's error quoted; "What changed" lists the changed packages with old and new versions. After an update the panel runs kyu-check so the tray and list are current.
- Reopening the panel mid-update shows the running screen with progress so far (replayed from the helper's log). Tested by killing the panel mid-update and starting it again, and by starting a second panel (it hands off to the first and exits).
- The panel reads history from the helper's log in `/var/log/kyu/`, not the journal; the log sketch's "full log stays in the system journal" line is superseded.
- `PanelController` is covered by `tests/test_panel.cpp` (list grouping, transaction progress, failures, start errors, lost helper). The QML itself has only been run against the preview stand-in, not real Kirigami.
- Not in this milestone: restart detection and .pacnew review (milestone 6), hold a package / update at shutdown (no mechanism yet), package details screen, prompts (milestone 4).

Open items carried forward:
- User-visible strings (tray and panel) use `QStringLiteral`, not KI18n. Without KF6 the tray falls back to `QSystemTrayIcon` (configure with `-DKYU_REQUIRE_KSNI=ON` for real builds). The KStatusNotifierItem code path has not been compiled yet, and the panel has not run with the real Kirigami, because the development container had no KF6. No PKGBUILD yet.
- The helper reports the real transaction size ("Packages (N)") but does not yet compare it with the checker's list before answering "Proceed" (see "Helper requirements").
- Prompt ownership (only the starting uid may answer) is recorded per transaction (`uid`), but there is nothing to answer until milestone 4.
