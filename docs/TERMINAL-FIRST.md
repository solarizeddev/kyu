# Terminal-first Kyu: what goes, what stays

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
| Updating | the terminal; the card says "Updating in Konsole" | Kyu's news and "Continue? [y/N]", one line "Do not close this window until Kyu says it is done", then pacman's and flatpak's own output |
| Done | the terminal's last lines, then the card | "Done: 6 packages, 2 Flatpaks." · "Restart needed: …" · "1 settings file to review: …" · "Press Enter to close." Card: "Restart to finish" · **Review** · **Restart** |
| Failed | the terminal | "pacman failed. No packages were changed; Flatpaks were not updated." · one sentence why · the fix command |

## Kept as is

| Part | Files | Why |
|---|---|---|
| Update checker | `src/checker/`, `src/core/pacmancheck.*`, `pacmanparse.*`, `updatestate.*`, `flatpakcheck.*`, `flatpakinstallations.h`, `flatpakwarnings.*`, `paths.*`, `jsonlog.*` | Knowing updates exist, without root, without a partial sync (checkupdates). |
| News | `src/core/news.*` | Arch news / Manjaro announcements since the last full upgrade. |
| Check scheduling | `src/core/checkscheduler.*`, `statewatcher.*`, `data/kyu-check.service.in`, `kyu-check.timer` | Periodic checks. |
| Tray icon | `src/tray/trayicon.*`, `badge.*` | The count, the tooltip, the Wayland activation token. |
| Tests | `test_kyucheck`, `test_news`, `test_pacmanparse`, `test_updatestate` | They test what stays. |
| Packaging | `packaging/arch/PKGBUILD` (simplified: no Rust, no corrosion) | |

## Changed

| Part | From | To |
|---|---|---|
| `kyu-update` (`src/client/`) | A D-Bus client of the root helper. | **The program that runs in the terminal**: pre-flight (news, `[y/N]`, a stale-lock check), then `sudo pacman -Syu` and, only if pacman succeeded, `flatpak update`. Then the post-flight lines and "Press Enter to close". pacman runs under `script` (util-linux), so its output stays fully interactive and is also recorded, for the failure explanation. It writes `~/.local/state/kyu/last-run.json` (result, restart, settings files) for the card. |
| Restart and old-library detection | `helper/src/aftercare.rs` (root) | C++ in `src/core/aftercare.*`, unprivileged: running vs installed kernel from `/proc` and `/usr/lib/modules`, and replaced libraries in your own processes' `/proc/PID/maps`. System services run as root and cannot be inspected without root; the line then says "programs you run". |
| Settings files | Collected by the helper from pacman's output | From `/var/log/pacman.log` for the run ("warning: … installed as ….pacnew"), readable without root. |
| Failure explanations | `helper/src/failure.rs` (classification) + `src/client/failuretext.*` (texts) | Both in C++ (`src/core/failure.*`), reading the recorded pacman output. The same kinds: network, mirror, keyring, files on disk, disk space, dependencies, lock. |
| Stale lock | The helper's `RemoveStaleLock` (root) | Pre-flight: if `/var/lib/pacman/db.lck` exists and no pacman runs, say so and print `sudo rm /var/lib/pacman/db.lck`. Kyu removes nothing itself. |
| The card (`kyu`, `src/panel/`) | Two full update UIs (Plasma and terminal looks), pages, models, about 5,600 lines of C++ and QML | One small card, under 300 lines: the three tray states above, plus the existing **diff** page for settings files (the one thing a terminal does badly). Theme colours only. |
| Tray click | Opened the panel | Opens the card. The card's button starts the terminal (KDE's preferred terminal through `KTerminalLauncherJob` when KIO is there, else `konsole`, else `xdg-terminal-exec`). |
| After the run | The tray watched the helper on D-Bus (`helperwatcher.*`) | The tray watches `last-run.json` and `/var/log/pacman.log`, then runs the checker again. |
| Test kit | `tools/kyu-demo.sh` (31 scenarios), `kyu-screens.sh`, `kyu-test-all.sh` | Rewritten small: the card's states and `kyu-update` against a fake `pacman`, `sudo` and `flatpak` in a temporary PATH. |

## Removed

| Part | Files (approximate size) | Why it can go |
|---|---|---|
| Root helper | `helper/` (Rust, 3,100 lines: D-Bus service, polkit, pacman on a pty, prompt parsing, transaction state, logs, aftercare, failure, lock) | pacman runs as itself in the terminal, under `sudo`. No prompt parsing, no auto-answered "Proceed?", nothing parsing pacman as root. |
| Helper system files | `data/io.github.solarizeddev.kyu.Helper.conf`, `.Helper.service.in`, `.Helper1.xml`, `.policy`, `kyu-helper.service.in` | No system service, no polkit action. |
| Helper D-Bus client | `src/client/helperclient.*` | |
| libflatpak updater | `src/flatpak/` (`kyu-flatpak`), `tests/flatpak_test.sh` | `flatpak update` itself, in the terminal. libflatpak stays in the checker for counting. |
| Update panel | `src/panel/updatesmodel.*`, `outputmodel.*`, `flatpakfollower.*`, `panelcontroller.*` (most), `qml/plasma/`, `qml/terminal/`, the output, changes and processes pages, `Notice`, `PrimaryButton`, `ButtonPair`, `LogTail`… | Replaced by the card and the terminal. |
| Their tests | `tests/helper_session_test.sh`, `tests/helper/fake-pacman` (reworked for the new tests), `test_helperclient`, most of `test_panel` | |
| Build pieces | Corrosion, Rust toolchain, `KYU_HELPER` option, `/usr`-prefix warning | |
| Docs | `docs/helper-protocol.md`; HANDOFF's helper sections become history | |

In all, roughly 9,000 lines go and about 1,000 new or ported lines come in.

## Risks and open points

- **Closing the terminal mid-update.** That sends SIGHUP to pacman, as in any terminal; Konsole asks first ("a program is still running"). Kyu prints "Do not close this window until Kyu says it is done" before pacman starts. Whether `kyu-update` can safely keep pacman alive (`nohup`-style) needs a test on a real machine; until then this is documented, not hidden.
- **sudo.** The run uses `sudo`, as people do by hand. A system without sudo for the user (pkexec only, or doas) needs a fallback: `pkexec` or `doas` when `sudo` is not there.
- **Terminal choice.** Konsole first; other desktops through `xdg-terminal-exec`. Tested on Konsole only.
- **Old libraries.** Only your own processes can be inspected without root (see above).
- **pamac users (Manjaro).** pamac keeps its own database lock and log; Kyu runs pacman, so the lock check covers both.
- **Translations.** pacman's output is in your language now (no `LC_ALL=C`). Kyu's own failure explanations still match pacman's English messages; in other languages Kyu shows the exit status and the last error lines instead of a classified explanation.

## Order of work

1. `kyu-update` as the terminal program: pre-flight, pacman under `script`, flatpak, post-flight, `last-run.json`. Port aftercare and failure classification to C++, with their tests.
2. The card, replacing the panel, and the tray changes (open the card, watch `last-run.json`).
3. Remove the helper, `kyu-flatpak`, the panel and their system files, tests and build pieces.
4. New small test kit, docs (README, TESTING, HANDOFF), PKGBUILD.
5. A real-machine run on the owner's Manjaro.

Each step gets a Fable review before the next.
