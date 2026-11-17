# Kyu: design handoff

> **History.** This is the original design (2026-11). Its root helper,
> polkit action and update panel were built and then removed when Kyu moved
> to running the real pacman in a terminal: see
> [`TERMINAL-FIRST.md`](TERMINAL-FIRST.md) and, for what Kyu is now,
> [`WHAT-KYU-SHOULD-BE.md`](WHAT-KYU-SHOULD-BE.md). The screenshots in
> `screenshots/` and the sketches in `sketches/` are of that old panel.

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
- **Prompt ownership.** Done in milestone 4: only the uid that started the transaction (or root) may answer or cancel; anyone may reattach and read. A pending question waits up to 30 minutes, then the helper stops pacman.
- **Fixed operations only.** The D-Bus API never takes client-supplied arguments, package lists or environment.
- **Lifetime.** Do not kill pacman on SIGHUP or service stop mid-transaction. Hold a logind inhibitor against shutdown and sleep while running. Handle a stale `db.lck`.
- **Show the real transaction.** The `checkupdates` list and the actual `-Syu` set can differ (mirror changed in between). Report the real set before committing.
- **Locale.** Parsing needs `LC_ALL=C`, which conflicts with UI rule 3 (quote pacman's messages): the quotes would be in English. Decide per message, or parse with C and show localized text separately.
- **Prompts are not a stable interface.** A `--print` pre-pass will not surface every question. Milestone 4 fails closed on unknown prompts; versions are not pinned. libalpm's question callback remains the robust alternative if pacman's wording churns.
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
7. **Failure states** (not sketched yet; done, see Status): out-of-sync mirror, keyring error, interrupted update and reattach, no network, lock file present.
8. **Later, optional:** upgrade simulation (dry run) before applying, watched packages, AUR as a separate step, settings.

## Open questions

- Manjaro specifics: branches, `pacman-mirrors`, and whether `checkupdates` behaves on Manjaro mirrors.
- How to answer pacman's interactive prompts reliably when driving the binary (pty and output parsing vs a `--print`/dry-run pre-pass to detect questions up front). See "Helper requirements" above.

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

**Milestone 3 (update panel): done.** Screenshots in `docs/screenshots/`, made with `tools/kyu-demo.sh` (then called `preview-panel.sh`).

- `kyu` is a standalone Qt Quick window using Kirigami for theme roles, units, icons and headings, and stock Qt Quick Controls for the rest (Plasma's `org.kde.desktop` style when installed). It is single-instance over the session bus. Clicking the tray icon, or "Show updates" in its menu, starts it.
- **List:** count, total package size, check time, check failures quoted; packages grouped by repo in pacman.conf order (kyu-check now sorts them that way and records each package's size from `pacman -Sup --print-format`), Flatpaks under their remote. "Update all", plus a menu with "Copy as command" (`sudo pacman -Syu`) and "Check again". Flatpaks are listed with a note that "Update all" does not update them yet (milestone 5).
- **Updating:** completed share of package operations, the stage or "upgrading N of M", the same list ticking off (done, working, waiting; packages pacman touched that the check did not list appear under "not in the last check"). "Closing this panel will not stop the update."
- **pacman output:** live output with Kyu's own lines (the command, the answered question) marked, pacman pid, root via polkit, the log path; "Copy log" and "Follow in Konsole" (runs `kyu-update --attach`).
- **Finished:** pacman's summary as the title, time, duration and exit code; on failure, pacman's error quoted; "What changed" lists the changed packages with old and new versions. After an update the panel runs kyu-check so the tray and list are current.
- Reopening the panel mid-update shows the running screen with progress so far (replayed from the helper's log). Tested by killing the panel mid-update and starting it again, and by starting a second panel (it hands off to the first and exits).
- The panel reads history from the helper's log in `/var/log/kyu/`, not the journal; the log sketch's "full log stays in the system journal" line is superseded.
- `PanelController` is covered by `tests/test_panel.cpp` (list grouping, transaction progress, failures, start errors, lost helper). The QML itself has only been run against the preview stand-in, not real Kirigami.
- Not in this milestone: restart detection and .pacnew review (milestone 6), hold a package / update at shutdown (no mechanism yet), package details screen.

**Milestone 4 (prompts): done.** Protocol: `docs/helper-protocol.md`, section "Questions".

- The helper recognises pacman's questions and relays them as structured `question` records. pacman then waits:
  - provider choice
  - package conflict
  - replacement
  - PGP key import
  - corrupted package file
  - packages that cannot be upgraded
  - IgnorePkg
- Each record carries pacman's text verbatim, its context lines (the provider list), the allowed answers, pacman's default, and the parsed facts (package names, key, file).
- New D-Bus methods: `Answer(transaction, question, answer)` and `Cancel(transaction)`. Only the uid that started the update, or root, may use them. Answers must be one of pacman's options. Cancel is accepted only while pacman waits, which is before it changes any package.
- No answer within 30 minutes: the helper stops pacman. Unrecognised questions still stop pacman (fail closed).
- Panel: a question screen per the ask sketch.
  - Header: "Paused, nothing changed yet" (or how many changed so far).
  - A title and one factual sentence per question type.
  - Options worded by what pacman will do. pacman's default is marked "pacman's default"; the sketch's hardware-based "Suggested" was not built (rule 7).
  - pacman's text verbatim.
  - Continue and "Cancel the update".
- "Skip these packages" is never offered: it is a partial upgrade (rule 4), so that question only offers Cancel.
- Other users see who may answer.
- `kyu-update` asks the same questions on the terminal.
- Tests: the parser against each pacman question format; the answer rules (wrong uid, wrong value, stale or repeated answers, root); end-to-end runs with a fake pacman for every type and path; and the panel answering and cancelling (preview screenshots).
- Not done: pacman versions are not pinned. A question whose wording changes in a future pacman falls into "unrecognised" and stops the update with nothing changed, rather than being answered wrongly. The parser is in `helper/src/parse.rs`, with a test per format.

**Milestone 5 (Flatpak in the same list): done.**

- `kyu-flatpak` (C++, libflatpak) updates every Flatpak with an update, in every system installation and the user installation, as the user, with no Kyu helper. System installations go through flatpak's own system helper and polkit rules.
- Like kyu-helper, it is a separate process with a JSON-lines log (`$XDG_STATE_HOME/kyu/flatpak/`) and a lock, so closing the panel does not stop it and a new panel reattaches to it.
- **Order:** pacman first, then Flatpaks (the running sketch's "flatpak next"). The panel starts `kyu-flatpak --after <pacman tx>` as soon as pacman's transaction exists, and kyu-flatpak waits for pacman by itself. So the Flatpaks still update if the panel closes during the pacman part.
- Flatpaks are skipped if the pacman update was cancelled or stopped at a question. They still update if pacman failed for other reasons, since the two are independent. With only Flatpak updates pending, "Update all" runs just kyu-flatpak.
- **Panel:** one run spanning both parts.
  - Flatpak rows tick off after the pacman rows.
  - "upgrading 4 of 9 · Flatpaks next", then "updating Flatpak 1 of 3".
  - The percentage covers both parts.
  - The output view interleaves both logs, and both log paths are shown.
  - The result combines both summaries ("3 upgraded · 2 Flatpaks updated"); on failure, both parts' results are listed.
- **Tray:** checks again when kyu-helper finishes a transaction (it listens for the helper's signals); kyu-flatpak runs kyu-check when it finishes.
- **Fixed on the way:** libflatpak treats an unreachable remote as non-fatal and reports "no updates" for it. Both kyu-check (since milestone 1) and kyu-flatpak now capture libflatpak's messages and report a failed check or update instead, quoting the error.
- **Tests:** `flatpak_update` runs kyu-flatpak against real libflatpak with a local repository and a throwaway installation built by `flatpak build-export`. It covers updates of apps and runtimes, nothing to do, an unreachable remote (updater and checker), the lock, `--after` ordering, and the skip after a cancelled pacman update. `test_panel` covers combined runs, Flatpak-only runs, cancel-and-skip, and a lost kyu-flatpak. The preview script now uses the same real repository, so the screenshots show a real Flatpak update.
- Not done: Flatpak updates have no cancel button (pacman's cancel is only at questions, and stopping between Flatpak refs has not been needed). "Follow in Konsole" (`kyu-update --attach`) shows the pacman part only. The test runs the user installation; system installations use the same code, through flatpak's system helper, which the container cannot run.

**Milestone 6 (after-update care): done.** Everything here is derived from the system (rule 7).

- **Before the update:** kyu-check marks the package that installed the running kernel. It reads `/usr/lib/modules/$(uname -r)/pkgbase`, so the list shows "restart needed" and the sketch's "restart" chip on that row; other installed kernels and the headers are not marked.
- **Distro news:** kyu-check picks the feed from `/etc/os-release`:
  - Arch, and Arch-based distros without their own feed (EndeavourOS, CachyOS), use `https://archlinux.org/feeds/news/`.
  - Manjaro uses its forum's stable/testing/unstable update announcements (branch from `/etc/pacman-mirrors.conf`). See "News, simplified" below for the feed URLs (changed after the redesign).

  It keeps items published since the last full upgrade, read from `pacman.log` (the last 30 days if there is none). The panel shows them above the list only when pacman updates are pending. A failed fetch is stated there and does not count as a failed check.
- **After the update:** the helper adds `configs` (from pacman's own `.pacnew`/`.pacsave` warnings) and `restart` to its finished record (see `docs/helper-protocol.md`, "After the update"). It derives:
  - a replaced running kernel (its modules directory is gone; the installed version comes from BOOT_IMAGE's pkgbase)
  - processes mapping replaced shared libraries, from `/proc/*/maps`, as root, so all processes are seen

  "Restart required" means the kernel was replaced or systemd (PID 1) maps a replaced library.
- **Panel, finished screen:**
  - The title becomes "12 updated, restart required", with "running … · installed …" under it (the sketch's line).
  - The restart card states the reason, with "Restart now" (Plasma's own reboot prompt via `org.kde.LogoutPrompt`, so applications can save; otherwise logind `Reboot` with polkit) and "I will restart later".
  - "N programs use replaced libraries" opens the list of programs and libraries.
  - "N settings files changed" opens the files, each with a `diff -u` view. Files only root can read say so. Merging is left to `sudo pacdiff` (copy button); Kyu does not write to `/etc`.
- `kyu-update` prints the same facts after an update.
- **Tests:**
  - restart detection against fake `/proc` and modules trees (Rust unit tests, and the session test through `--proc-dir`/`--modules-dir`, session mode only)
  - `.pacnew`/`.pacsave` parsing
  - feed parsing (RSS and Atom), distro-to-feed mapping and `pacman.log` reading
  - kyu-check with a local feed and a fake modules directory
  - the panel's list and finished states, and the diff
- Not done: news is not "only before big updates". It is shown whenever there are pacman updates and news newer than the last full upgrade. Arch's feed only carries announcements that matter for updates, so a size threshold seemed like guessing.

**Milestone 7 (failure states): done.**

- **Failure kinds:** the helper classifies a failed transaction from pacman's own lines into `network`, `mirror` (404/410 on package files), `keyring`, `files` (exists in filesystem), `disk`, `dependencies` or `lock`, and keeps the evidence lines verbatim (`docs/helper-protocol.md`, "Failures").
- **What the user sees:** the panel's finished screen and `kyu-update` explain each kind in a fact-based way, show what it is about (hosts, files, packages) and pacman's lines, and give the usual fix as a command to copy.
  - Out-of-date mirror on Manjaro: `pacman-mirrors --fasttrack`; on Arch, a pointer to `/etc/pacman.d/mirrorlist`.
  - Keyring: `pacman -Sy --needed <installed *-keyring> && pacman -Su`.
  - Disk space: `paccache -r`.
  - Kyu does not run these itself.
- **Lock file:** libalpm holds `db.lck` open for a whole transaction, so the helper finds the holder through `/proc/*/fd`. `StartUpdate`'s refusal names the holder, or says that nothing holds the lock. `RemoveStaleLock` (same polkit action) removes it only when no process holds it and no `pacman` process runs. The panel offers "Remove the stale lock" in exactly that case; `kyu-update --remove-stale-lock` does the same.
- **Interrupted update:** when the helper's newest log ends without a result, the panel's list says so, with the log path and the fact that Update all runs `pacman -Syu` again, which completes the update.
- **No network for checks:** checkupdates and libflatpak failures were already quoted on the list (milestones 1 and 5).
- **Tests:** classification against recorded pacman 7 lines (Rust); lock holder detection with a fake `/proc` (Rust); end to end with a fake pacman for each failure kind, a stale lock removed and a held one refused (session test); the panel's failure, interrupted and lock states.

**Review of milestones 1-6 (by Fable, read-only, after milestone 7).** Each finding was checked against the code; fixed and tested unless noted.

- **Fixed: question detection could SIGINT pacman mid-commit** (a hook or scriptlet ending a partial line in `[y/N] `). Questions are now looked for only before `:: Processing package changes...` and only after 150 ms of quiet; afterwards the helper never writes to or signals pacman. Test: a hook line ending in `[y/N]` during the commit stage.
- **Fixed: the skip question accepted "y" over D-Bus** (rule 4 was enforced only in the panel). The helper now gives it no answers; only Cancel. Test: `Answer y` refused, then cancelled.
- **Fixed: pacman could be killed by SIGPIPE if the helper died; pacman's stdout was block-buffered on a pipe** (progress in bursts, and a provider list possibly stuck behind the question). pacman now writes to a pseudo-terminal for stdout and stderr (line-buffered, one ordered stream; EIO instead of SIGPIPE), stdin stays a pipe, and SIGPIPE is ignored in pacman.
- **Fixed: parser mismatches with real pacman.**
  - the `:: ` prefix pacman's `question()` adds (the skip question)
  - conflict names with versions
  - key import without a uid, and the older "created:" form
  - the corrupted-file line no longer has to be the very last line

  Tests for each format.
- **Fixed: HelperClient kept the previous transaction's sequence number**, so a second update in the same panel session showed nothing or dropped records. Test: two logs followed one after the other.
- **Fixed: the helper could exit as idle while the polkit dialog was open** (calls in flight now count as activity). **SIGTERM with a question pending** now cancels at that safe point instead of waiting up to 30 minutes (test). **A panic in the transaction task** now ends with a `finished` record instead of leaving the helper "running".
- **Fixed: `pacman -Sup` in kyu-check could wait on stdin** for 60 s on every check (now `--noconfirm` and stdin closed). **News after a failed or stopped upgrade:** the cutoff is now the last upgrade that completed (test).
- **Fixed: privacy.** The world-readable finished record named every user's processes; it now names only the starting user's and system processes, and counts the rest.
- **Fixed: feed links** open only for http(s). **Restart now** calls Plasma's on-demand `org.kde.LogoutPrompt` instead of checking whether it is already running.
- **Partly addressed: hook or scriptlet lines that look like package operations** (`installing foo...`) are now counted only during the commit stage; a scriptlet printing exactly that line during commit would still be counted.
- **Noted: `CMAKE_INSTALL_LIBEXECDIR`** defaults to `/usr/libexec`; an Arch package should set it (README).
- **Not verifiable here:** the exact pacman 7 wording of the conflict question, and whether the provider list was ever stuck behind the question; both are covered by the changes above, but need a run against real pacman.

**Terminal-style redesign (after milestone 7).** The user asked for a "more modern, terminal, Omarchy-like" look. Fable and I both judged a full Omarchy skin wrong for a KDE app (its own palette breaks rule 6, all-monospace hurts prose and translations), but the direction worth taking. The panel now uses one layout, with no theme setting:

- **Flat, dense layout:** flat sections with 1px dividers instead of rounded cards. Rows are about 1.6 grid units high, with name, versions or status, and size in fixed-width columns.
- **Headings and explanations** stay in the normal font. Status always carries a mark or a word, never colour alone. There is no box-drawing or ASCII decoration.
- **Command preview:** the exact command "Update all" runs is shown as text above the button (rule 3).
- **Updating screen:** a live 5-line tail of the output under the list; the full output is one key away.
- **Keyboard:** shortcuts and one hint line per screen. Enter, 1–9, c, l, s, p, w, y and Esc. No key restarts or cancels.
- **Colours:** from Kirigami.Theme roles only. With a Tokyo Night or Catppuccin colour scheme and a monospace system font it reads like Omarchy; with Breeze it reads like a KDE utility.

The sketches in `docs/sketches/` show the earlier card look; the screenshots in `docs/screenshots/` show the current one.

**News, simplified (after the redesign).** Fable and I reviewed whether the news feed was worth keeping, after the user asked whether a button would do:

- **Arch, CachyOS, EndeavourOS** (ID or ID_LIKE `arch`): kyu-check fetches Arch's news feed only. Arch's news is the "manual intervention" channel, rare and the one warning pacman cannot give.
- **Manjaro:** kyu-check fetches the RSS feed of the branch's update announcements on Manjaro's forum (Discourse): `https://forum.manjaro.org/c/announcements/<branch>-updates/<id>.rss`, ids 12 stable, 13 testing, 15 unstable; on arm64 `/c/arm/<branch>-updates/` with 79, 80, 81. The ids come from Manjaro's own news app (`oguzkaganeren/manjaro-app`, `NewsScreen.tsx`), which reads the same categories as `.json`; other projects use the `.rss` form. Each batch has one post whose title names what changed ("[Stable Update] 2026-08-23 - Kernels, KDE Framework, Firefox, …") and whose body lists known issues, so the row shows exactly when a batch is pending and goes away after the upgrade. Arch's news is not shown on Manjaro: it is mistimed for the delayed branches. (First version: a link only, with an unverified URL; replaced after the user pointed at Manjaro's widget code.) **Not fetched from this environment** (the forum is blocked here); the parser is tested on a Discourse-shaped feed.
- **Other distributions:** no news.
- **The panel:** shows one row above the list only when pacman updates are pending and there is something to show. It reads "Arch Linux news · N since your last upgrade" above the newest title, which gets the full width because it is often the warning itself. Clicking opens that post, or the news page if there are several. A failed fetch is one muted line ("Arch Linux news not checked: …"). The bordered box, the per-distro feed table and the title list are gone. A plain "open news" button was rejected because it states no fact: someone who never clicks it learns nothing.

**Scaling and resizing (after the news change).** Tested offscreen with `tools/demo/state-long.json` (154 updates, long names, git snapshot versions, a restart, a long check error, a long news title). Sizes: the minimum window, the default, 1200×700, fonts at 14 and 18 pt, and `QT_SCALE_FACTOR=2`; list, updating, question and finished screens. Fixes:

- **Window:** the default and minimum sizes are clamped to 90% of the screen's free area, so a large font cannot push the buttons off a small screen. The minimum is 18×25 grid units. On wide windows the content stays a centred column of at most 44 grid units.
- **Package rows:** column widths come from the text measured in its own fixed-width font (`TextMetrics`); the old widths came from the default font and elided short names. With room, the name and versions show in full. When short of space, the name keeps at least 4 grid units and ends in "…", and the versions are cut from the left, so the new version stays visible. The restart chip is never covered. Below 22 grid units the size column hides. Hovering a cut row shows the full text.
- **Messages:** a long error shows 3 lines; clicking it shows the rest. The list keeps at least two rows visible, and the footer buttons are always visible.
- **Questions:** the option name stays on one line (up to 60% of the row); the detail ("extra · pacman's default") wraps first.
- **News row:** the source sits on its own line so the title gets the full width.

To repeat: `SIZE=270x375 OUT=out.png tools/kyu-demo.sh list-long` (also `FONT_PT=18`, or `SCALE=2`), or all at once with `tools/kyu-screens.sh`. Not tested: a real Plasma session, fractional scaling under Wayland, right-to-left languages.

**Ready for a real system (after the Manjaro feed).** The owner tests on Arch + Plasma 6 next; [`docs/TESTING.md`](TESTING.md) is the procedure.

- **Test kit:** the screenshot and key options moved from the preview build into the real panel (`src/panel/testdriver.cpp`): window size, screenshot, opening a page, and, only against a session-bus test helper, pressing Update all, answering and pressing keys. `tools/kyu-demo.sh` runs the real panel against a simulated system: a test helper on a private bus, the fake pacman (its package list now configurable), a throwaway Flatpak installation, a fake checker (`KYU_CHECK`, also honoured by kyu-flatpak), a fake `/proc` and modules directory. It has 31 scenarios, with sample states in `tools/demo/`. Colour scheme, font size, scale and window size apply through a private `kdeglobals`. `tools/kyu-screens.sh` screenshots the matrix, and `tools/kyu-test-all.sh` builds, runs ctest and the matrix, records package versions and zips the result (private paths scrubbed). "Restart now" never restarts in session-bus mode (test). Fable reviewed the kit twice; its fixes: Flatpak isolation from `/etc/flatpak` (`FLATPAK_CONFIG_DIR`, also in the Flatpak test), Ctrl+C cleanup, waiting for the helper's name, the zip staged in a temporary directory (the first version deleted the directory it was given), screenshot failures in the result.
- **Pacman's real transaction:** when "Packages (N)" differs from the list the update started with, the updating and result screens say so ("new dependencies, or the mirrors changed"), and point to pacman's own list in the output (test). This closes the "Show the real transaction" requirement as a report. The helper still answers "Proceed" itself: the API takes no package list (rule: fixed operations only).
- **Packaging:** `packaging/arch/PKGBUILD` builds the checkout in place. A staged install (`DESTDIR`) put every file where dbus, polkit, systemd and XDG autostart read it.
- **Second review (Fable, before the real-machine test), fixed:**
  - The autostart file went to `/usr/etc/xdg/autostart`: it now uses the full sysconfdir (`/etc`). Configuring the helper with a prefix other than `/usr` now warns, because dbus and polkit read only from `/usr/share` and `/etc`.
  - The question's digit and Enter shortcuts stayed live under the output page, so Enter could answer a question nobody saw. They are now active only while the question is on screen, and Enter on a focused push button clicks it ("Cancel the update", "Check again"). On Qt 6.4 `instanceof QQC2.AbstractButton` is false for Buttons, so the check is duck-typed (`Style.pressable`).
  - Secondary text used `Kirigami.Theme.disabledTextColor`, which real colour schemes draw fainter than the stand-in. It is now the text colour at 70% (`Style.muted`).
  - A tray click could not focus an open panel on Wayland. The click's activation token now goes to the panel, through `XDG_ACTIVATION_TOKEN` for a new panel, or `Activate(token)` for a running one, using KWindowSystem when available.
  - The tray drew the count into a pixmap, coloured for the application palette rather than Plasma's panel. The icon is now set by name, with the count as an overlay.
  - Manjaro ARM (`ID=manjaro-arm`) got Arch's news; pamac's full upgrades did not count as the last upgrade. Both are fixed (tests).
  - Found while testing: the result message elided an empty line, and with the 3-line cap hid "2 Flatpaks updated". The update result is now shown in full.
- **Not verified (no KF6 here):** `KStatusNotifierItem::providedToken()`, `KWindowSystem::setCurrentXdgActivationToken`/`activateWindow`, `Kirigami.Theme.fixedWidthFont`, how qqc2-desktop-style draws `highlighted` buttons, and running offscreen with the KDE platform theme.

**First real-system run, and two looks (after the test kit).** The owner ran `tools/kyu-test-all.sh` on Manjaro (Plasma 6.7.5, KF 6.30, Qt 6.11.2, flatpak 1.18, Wayland). Everything compiled, including the KF6 tray and window activation. Fixed from that run:
- A system Flatpak installation with no repository (nothing installed) made kyu-check and kyu-flatpak fail, because newer libflatpak cannot open it and asks its root helper to create it. Installations without a repository are now skipped (`Kyu::flatpakInstallations`, test).
- `test_panel` assumed C number formatting; the panel is right to show "165,0 MiB" in German. The test pins the locale.
- Output was coloured by stream, so libflatpak's normal stderr chatter showed as warnings. Lines are now coloured by what they say (`OutputModel::levelOf`, test).
- "Update failed" when only the Flatpaks failed: the title is now "N upgraded · Flatpak update failed" (`outcome` property, test).
- Large fonts broke fixed row heights. All heights now come from the content, and the window size from the font actually in use.
- The demo showed the checkout path under the home directory; it now runs copies of the binaries.

The owner found the UI "quite average". Fable and I agreed (no focal point, a wall of small grey monospace, primary buttons that Breeze draws like any other). Three directions were sketched; the owner chose to have both the native one and the full-terminal one:
- **Plasma look** (`qml/plasma`, default): header with a status icon and the one fact that matters, problems as notices (`Notice`: the scheme's negative, neutral, positive and active colours), news as an information strip, two-line rows on a View surface, an empty state, one accent "Update all" (`PrimaryButton`: Breeze ignores `highlighted`), the commands one menu item away, a progress bar, the output folded into one line, the outcome first after an update, `ButtonPair` (a Flow, so buttons never run past the window).
- **Terminal look** (`qml/terminal`): everything in the fixed-width font on the View background, a status line, a key line (`KeyButton`), bordered boxes, aligned columns with ✓ ▸ · marks, a block progress bar, the live output. Accents use the link colour (readable on light schemes) and fills the selection colour, so it follows the scheme.
- The look is `Panel.style`, saved in `~/.config/kyurc` (`[Panel] Style=`), switched from the menu, or `--look` for one run (QApplication takes `--style`). The pages and keys are shared. `tools/kyu-screens.sh` captures both looks.
- Fable reviewed each look. Fixed: buttons that overflowed narrow windows, the accent used for information, icon sizes, the disabled main button, notices cut at three lines, words split mid-word. Qt 6.4 lessons: an eliding label's `implicitWidth` is unreliable (measure with `TextMetrics`); a fixed-width item's minimum pushes on its parent layouts; a layout that changes its columns from its own width leaves stale positions; `-1` is not "no maximum" (use `Infinity`).

Open items carried forward:
- User-visible strings (tray and panel) use `QStringLiteral`, not KI18n.
- Milestone 8 (dry run, watched packages, AUR, settings) is not started.
