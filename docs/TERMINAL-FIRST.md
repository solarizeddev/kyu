# Terminal-first Kyu: what goes, what stays

> **Decision record (2026-11-16).** The tables below are the plan as decided
> then. The card's words, layout and states have changed since (designed
> with Fable over several real-machine rounds): see the README for what Kyu
> does now, and [`WHAT-KYU-SHOULD-BE.md`](WHAT-KYU-SHOULD-BE.md) for its
> status.

Decided 2026-11-16 by the owner, after the second real-system test: the update panel was "too complicated, too much text, too confusing. I'd rather use the terminal than this tool." Fable and I reached the same conclusion: on Arch the terminal is the trusted interface. Kyu's value is everything around the update, not re-drawing it.

Sketches (direction A): https://claude.ai/artifact/J5AmuUGK3MBmFpU5SCKsK7

## What changes in the goal

HANDOFF.md set out to be "one update flow that survives a UI crash", unlike tools that update in a terminal. That goal is dropped. The update now runs in the user's terminal, as the real commands:

```
sudo pacman -Syu
flatpak update
```

Kyu does three things around them:

1. **Tells you.** A tray icon with the count, and a small card: how many updates, whether a restart will be needed, and news that needs reading first.
2. **Frames the run.** Before pacman: the news since your last upgrade, with "Continue? [y/N]" when there is any. After: what is left to do (restart, settings files, programs on old libraries), or why it failed and the command that usually fixes it.
3. **Remembers.** After the terminal closes, the card says "Restart to finish" until you do, and opens the settings-file diff.

Unchanged rules: no partial upgrades (only `-Syu`), nothing hidden (you see pacman's own output and answer its own questions, including "Proceed?"), facts over reassurance, colours from the theme.

## The three moments

| Moment | Where | What the user reads |
|---|---|---|
| Updates available | tray icon and card | "8 updates" · "Restart needed afterwards" · (only with news) "Read first: <title>" · **Update in Konsole** |
| Updating | the terminal; the card says "Updating in Konsole" | Kyu's news and "Continue? [y/N]", one line "Do not close this window until Kyu says it is done", then pacman's and flatpak's own output, with their own prompts |
| Done | the terminal's last lines, then the card | "Done: 6 packages, 2 Flatpaks." · "Restart needed: …" · "1 settings file to review: …" · "Press Enter to close." Card: "Restart to finish" · **Review** · **Restart** |
| Failed | the terminal | "pacman failed. No packages were changed; Flatpaks were not updated." · one sentence why · the fix command |

With only Flatpak updates pending, `kyu-update` skips sudo and pacman and runs `flatpak update` alone.

## Kept as is

| Part | Files | Why |
|---|---|---|
| Update checker | `src/checker/`, `src/core/pacmancheck.*`, `pacmanparse.*`, `updatestate.*`, `flatpakcheck.*`, `flatpakinstallations.h`, `flatpakwarnings.*`, `paths.*`, `jsonlog.*` | Knowing updates exist, without root, without a partial sync (checkupdates). `UpdateState` keeps its fields (the format is versioned), even those only the old list used. |
| News | `src/core/news.*` | Arch news / Manjaro announcements since the last full upgrade. |
| Check scheduling | `src/core/checkscheduler.*`, `statewatcher.*`, `data/kyu-check.service.in`, `kyu-check.timer` | Periodic checks. |
| Tray icon | `src/tray/trayicon.*`, `badge.*`, `src/core/statustext.*` (tooltip) | The count, the tooltip, the Wayland activation token. |
| Window plumbing | `src/panel/singleinstance.*` (a second tray click focuses the card), the optional KF6WindowSystem activation | |
| Desktop files | `data/kyu.desktop.in`, `data/kyu-tray.desktop.in` | |
| Dev tools for the card | `src/panel/testdriver.*`, `src/panel/preview/` (Kirigami stand-in: the dev container has no KF6), `tools/demo/state-*.json`, `tools/demo/fake-kyu-check` | Screenshots of the card's states. |
| Tests | `test_kyucheck`, `test_news`, `test_pacmanparse`, `test_updatestate` | They test what stays. |

## Changed

| Part | From | To |
|---|---|---|
| `kyu-update` (`src/client/`) | A D-Bus client of the root helper. | **Rewritten** as the program that runs in the terminal, linked to `kyucore`. Pre-flight: news and `[y/N]`, a stale-lock check, a pid file so two runs cannot overlap (and so the card can say "Updating in Konsole"). Then `script -q -e -f -c "sudo pacman -Syu" <file>` (util-linux): pacman stays fully interactive, its exit code comes back, and its output is recorded for the failure explanation (the password is never echoed, so never recorded). Only if pacman succeeded: `flatpak update`, with its own prompts and exit code. Then the post-flight lines, `last-run.json` for the card, and "Press Enter to close". |
| Restart detection | `helper/src/aftercare.rs` (root) | C++ in `src/core/aftercare.*`, unprivileged. The kernel: running vs installed, from `/proc` and `/usr/lib/modules`. systemd: as root the helper read PID 1's maps; unprivileged it cannot, so "restart required" also follows from `systemd` (or glibc) being in the run's upgraded packages, from `/var/log/pacman.log`. |
| Programs on old libraries | The helper read every process's maps (root) | Your own processes only (`/proc/PID/maps` of other users is unreadable). The line says "programs you run". |
| Settings files | Collected by the helper from pacman's output | From the run's lines in `/var/log/pacman.log` ("[ALPM] warning: /etc/x installed as /etc/x.pacnew"), with a fallback that looks for `*.pacnew`/`*.pacsave` files newer than the run's start, in case the log is ever written translated. |
| Diff of a settings file | `PanelController::diff()` (`diff -u`) and its QML page | Ported to the card, about 40 lines. Files readable only by root keep the existing hint (`sudo pacdiff`). |
| Failure explanations | `helper/src/failure.rs` (classification) + `src/client/failuretext.*` (texts) | Both in C++ (`src/core/failure.*`), reading the recorded output after stripping carriage returns and progress bars. The same kinds: network, mirror, keyring, files on disk, disk space, dependencies, lock. `/var/log/pacman.log` alone is not enough: most errors before the transaction (keyring, mirror, conflicts) are not written there. |
| Stale lock | The helper's `RemoveStaleLock` (root) | Pre-flight: if `/var/lib/pacman/db.lck` exists and no pacman runs, say so and print `sudo rm /var/lib/pacman/db.lck`. Kyu removes nothing itself. |
| The card (`kyu`, `src/panel/`) | Two full update UIs (Plasma and terminal looks), pages, models, about 5,600 lines of C++ and QML | One small card, under 300 lines: the three tray states above, plus the diff page. Theme colours only. |
| Tray | Opened the panel; watched the helper on D-Bus (`helperwatcher.*`) | Opens the card. Watches `last-run.json` and the update lock instead; `kyu-update` runs the checker again when it ends. `src/tray/main.cpp` changes; `helperwatcher.*` goes. |
| Starting the terminal | — | `konsole -e kyu-update`, else `xdg-terminal-exec kyu-update`. (KIO's `KTerminalLauncherJob` would honour the user's chosen terminal, but adds KIO for one call; later, if wanted.) |
| Packaging | PKGBUILD with Rust, corrosion, polkit | Drops `rust`, `corrosion`, `polkit`; adds `util-linux` (in base) and `diffutils`. The card is Qt Widgets (no Kirigami). The package removes the old D-Bus, polkit and systemd helper files; helper logs in `/var/log/kyu` are left in place. |
| Test kit | `tools/kyu-demo.sh` (31 scenarios), `kyu-screens.sh`, `kyu-test-all.sh` | Rewritten small: the card's states, and `kyu-update` against fake `pacman`, `sudo` and `flatpak` in a temporary PATH. |

## Removed

| Part | Files (approximate size) | Why it can go |
|---|---|---|
| Root helper | `helper/` (Rust, 3,100 lines: D-Bus service, polkit, pacman on a pty, prompt parsing, transaction state, logs, aftercare, failure, lock) | pacman runs as itself in the terminal, under `sudo`. No prompt parsing, no auto-answered "Proceed?", nothing parsing pacman as root. |
| Helper system files | `data/io.github.solarizeddev.kyu.Helper.conf`, `.Helper.service.in`, `.Helper1.xml`, `.policy`, `kyu-helper.service.in` | No system service, no polkit action. |
| Helper D-Bus client | `src/client/helperclient.*` | |
| libflatpak updater | `src/flatpak/` (`kyu-flatpak`), `tests/flatpak_test.sh` | `flatpak update` itself, in the terminal. libflatpak stays in the checker for counting. |
| Update panel | `src/panel/updatesmodel.*`, `outputmodel.*`, `flatpakfollower.*`, `panelcontroller.*` (all but the diff), `qml/plasma/`, `qml/terminal/`, the output, changes and processes pages, `Notice`, `PrimaryButton`, `ButtonPair`, `LogTail`… | Replaced by the card and the terminal. |
| Tray helper watcher | `src/tray/helperwatcher.*` | |
| Their tests | `tests/helper_session_test.sh`, `test_helperclient`, most of `test_panel`; `tests/helper/fake-pacman` is reworked for the new tests | |
| Build pieces | Corrosion, the Rust toolchain, the `KYU_HELPER` option, the `/usr`-prefix warning | |
| Docs | `docs/helper-protocol.md`; HANDOFF's helper sections become history | |

In all, roughly 9,000 lines go and about 1,000 new or ported lines come in.

## Risks and open points

- **Closing the terminal mid-update** sends SIGHUP to pacman, as in any terminal; Konsole asks first ("a program is still running"). pacman installs its own signal handlers, so a wrapper cannot simply ignore HUP for it, and detaching it (`setsid`, `systemd-run --scope`) would lose the terminal. Kyu prints "Do not close this window until Kyu says it is done" before pacman starts; this stays documented behaviour.
- **sudo.** The run uses `sudo`, as people do by hand. Without sudo: `doas` if present, else `pkexec` (which shows a password dialog instead of a prompt in the terminal).
- **Terminal choice.** Konsole first, then `xdg-terminal-exec`. Tested on Konsole only.
- **Old libraries** of system services cannot be seen without root (see above).
- **pamac users (Manjaro).** pamac uses pacman's database lock, so the lock check covers it.
- **Translations.** pacman's output is in your language now (no `LC_ALL=C`). Kyu's failure classification matches pacman's English messages; in other languages Kyu shows the exit status and pacman's last error lines instead of a classified explanation.

## Order of work

Status (2026-11-17): 1, 3 and 4 done; 2 and the second run of 5 still need
the owner's machine. The remaining work is the list in
[`WHAT-KYU-SHOULD-BE.md`](WHAT-KYU-SHOULD-BE.md).

1. `kyu-update` as the terminal program: pre-flight (news, lock, pid file, Flatpaks-only path), pacman under `script`, flatpak, post-flight, `last-run.json`. Port aftercare (with the systemd rule) and failure classification to C++, with their tests.
2. **A real-machine run of `kyu-update` on the owner's Manjaro**, before anything is deleted.
3. The card, replacing the panel (with the diff ported), and the tray changes.
4. Remove the helper, `kyu-flatpak`, the panel and their system files, tests and build pieces; PKGBUILD.
5. New small test kit, docs (README, TESTING, HANDOFF), and a second real-machine run.

Each step gets a Fable review before the next.
