# kyu-helper protocol

The contract between `kyu-helper` (Rust, root) and its clients (`kyu-update` now, the panel from milestone 3). The interface XML is `data/io.github.solarizeddev.kyu.Helper1.xml`; this file describes the JSON carried in it. Format version: **1** (the `v` field).

## Bus names

| | |
|---|---|
| Bus | system (`--session` for development) |
| Service | `io.github.solarizeddev.kyu.Helper` (D-Bus activated, systemd unit `kyu-helper.service`) |
| Object | `/io/github/solarizeddev/kyu/Helper` |
| Interface | `io.github.solarizeddev.kyu.Helper1` |
| polkit action | `io.github.solarizeddev.kyu.update` (`auth_admin_keep` for active sessions) |

Only root may own the service name. Anyone may call it. `GetState` and the signals are open to all local users, like `/var/log/pacman.log`.

## Methods

**`StartUpdate() → s transaction`.** Runs `pacman -Syu --noprogressbar --color never` with `LC_ALL=C` and an empty environment apart from `PATH`. It takes no arguments: callers cannot choose packages, options or environment. polkit may show a password dialog, so call it with a long timeout (the Qt client uses 10 minutes). Errors (`io.github.solarizeddev.kyu.Helper1.Error.*`):

| Error | Meaning |
|---|---|
| `NotAuthorized` | polkit said no, or the password dialog was cancelled. |
| `Busy` | A Kyu transaction is already running. |
| `Locked` | pacman's `db.lck` exists: another package manager is running, or an earlier run was interrupted. |
| `Failed` | Anything else; the message says what. |

**`Answer(s transaction, t question, s answer)`.** Answers a relayed question: `question` is the record's `id`, `answer` one of its `answers[].value`. Only the uid that started the transaction, or root. Errors:

| Error | Meaning |
|---|---|
| `NoQuestion` | That transaction is not running, pacman is not waiting on that question, or it was already answered. |
| `NotAllowed` | The caller is not the user who started the update (or root). |
| `InvalidAnswer` | Not one of pacman's answers to this question; pacman keeps waiting. |

**`Cancel(s transaction)`.** Stops pacman (SIGINT) while it waits for an answer. Refused with `NoQuestion` at any other time: every pacman question comes before it changes a package, so this is the one point where stopping it is safe. Same `NotAllowed` rule as `Answer`.

**`GetState() → s json`.** For example:

```json
{
  "v": 1,
  "phase": "running",
  "seq": 42,
  "transaction": {
    "tx": "20261116T101723Z-00", "uid": 1000, "startedAt": "2026-11-16T10:17:23Z",
    "log": "/var/log/kyu/20261116T101723Z-00.jsonl",
    "stage": "commit", "current": 4, "total": 9,
    "action": "upgrading", "package": "mesa",
    "summary": "", "question": null
  }
}
```

- `phase` is one of `idle`, `running`, `finished` or `failed`.
- `transaction` is the current or last transaction since the helper started. `question` is the pending question record (see below) or `null`.
- After the helper has exited for being idle and been restarted, there is no `transaction`. Instead there is `previous`: `{tx, log, interrupted, success, summary}`, read from the newest log file. `interrupted: true` means that log has no `finished` record: the helper died mid-update.

## Signal

**`Event(s transaction, t seq, s record)`.** `record` is exactly the line appended to the log file.

## Log

`/var/log/kyu/<transaction>.jsonl`, mode 0644, one JSON object per line. The newest 30 logs are kept. Transaction ids are `YYYYMMDDTHHMMSSZ-NN` (UTC plus a counter), so file names sort by time.

Every record has `v`, `tx`, `seq` (1, 2, 3, … per transaction), `time` (UTC, RFC 3339) and `kind`:

| kind | Fields | When |
|---|---|---|
| `started` | `uid`, `command` (array), `pid` | pacman started (no `pid` if it could not start) |
| `output` | `stream` (`stdout`/`stderr`), `text` | every line pacman prints, verbatim |
| `stage` | `stage`: `sync`, `resolve`, `download`, `check`, `commit`, `hooks` | pacman starts a new step |
| `packages` | `total` | pacman's "Packages (N)" line: the real transaction size |
| `progress` | `action`, `package`, `current`, `total` | before each package operation |
| `question` | `id`, `handled: true`, `type`, `text`, `context`, `answers`, `default`, `details`, `timeoutSeconds` | pacman waits for an answer; see Questions |
| `question` | `text`, `handled: false` | a question Kyu does not recognise; the helper stops pacman with SIGINT |
| `answered` | `question`, `answer`, `reason` | the helper answered "Proceed with installation?" itself |
| `answered` | `id`, `question`, `answer`, `uid` | a user answered a relayed question |
| `cancelled` | `id`, `uid` | a user cancelled at a question; pacman gets SIGINT |
| `timeout` | `id`, `seconds` | nobody answered in time; pacman gets SIGINT |
| `finished` | `success`, `exitCode`, `summary`, `changed` | last record |

`summary` states what happened. Examples:
- `3 upgraded`
- `Nothing to do`
- `pacman: failed to commit transaction (conflicting files). 2 upgraded before the error.`
- `Cancelled at pacman's question "…". No packages were changed.`
- `Stopped at a question Kyu does not recognise: "…". No packages were changed.`

"No packages were changed" is stated only when no package operation was seen in pacman's output.

## Following a transaction without gaps

1. Subscribe to `Event`.
2. Call `GetState`. If it has a `transaction`, read its `log` file, delivering records in `seq` order.
3. Deliver signals with a `seq` above the last one delivered. On a gap, read the log again.
4. Stop at `finished`. If the service leaves the bus first, the helper died. Report it; the log ends without a result.

`HelperClient` (`src/client/helperclient.cpp`) implements this.

## Questions

pacman prints questions on stderr without a newline and waits on stdin. The helper reads them like this:

- **`:: Proceed with installation? [Y/n]`**: answered `y` by the helper, because the user chose "Update all". This produces an `answered` record with a `reason`.
- **Recognised questions:** these are relayed as a `question` record, and pacman waits:

  | `type` | pacman asks | `details` |
  |---|---|---|
  | `provider` | `:: There are N providers available for DEP:` … `Enter a number (default=1):` | `dependency`; `answers` are the numbers, with `label` = package and `detail` = repo |
  | `conflict` | `:: A and B are in conflict. Remove B? [y/N]` | `install`, `remove` |
  | `replace` | `:: Replace OLD with REPO/NEW? [Y/n]` | `old`, `new` |
  | `import_key` | `:: Import PGP key FPR, "UID"? [Y/n]` | `key`, `uid` |
  | `corrupted` | `:: File F is corrupted (R).` / `Do you want to delete it? [Y/n]` | `file`, `reason` |
  | `skip` | `… cannot be upgraded due to unresolvable dependencies` / `Do you want to skip …? [y/N]` | `packages` |
  | `ignored` | `P is in IgnorePkg/IgnoreGroup. Install anyway? [Y/n]` | `package` |

  For yes/no questions, `answers` is `y` and `n`. `default` is pacman's own default, read from `[Y/n]`, `[y/N]` or `(default=N)`. `text` is the question line verbatim, and `context` holds the lines pacman printed for it, such as the provider list.

  The panel offers no answer for `skip`: answering yes would be a partial upgrade (HANDOFF.md rule 4). Only `Cancel` is offered.
- **Anything else** is a `question` record with `handled: false`, and pacman gets SIGINT. The helper fails closed rather than guessing.

pacman waits up to 30 minutes for an answer (`--question-timeout`). After that the helper stops it, the same as a cancel. Only the uid that started the transaction, or root, may answer or cancel. A client that attaches while pacman waits sees the `question` record in the log replay, and can answer it.

## kyu-flatpak's log

`kyu-flatpak` writes the same record format to `$XDG_STATE_HOME/kyu/flatpak/<tx>.jsonl` (newest 30 kept), with `"source": "flatpak"` on every record. It holds `$XDG_STATE_HOME/kyu/flatpak.lock` while it runs, so there is one at a time, and a panel finds the running one through that lock's pid.

| kind | Fields | When |
|---|---|---|
| `started` | `pid`, `uid`, `command`, `log`, `after` | first record |
| `stage` | `stage`: `waiting` (for pacman, with `--after`) or `flatpak` | |
| `output` | `stream`, `text` | Kyu's own lines and libflatpak's messages, verbatim |
| `packages` | `total` | operations to run (refs with updates; more once related refs are resolved) |
| `progress` | `action` (`updating`, `installing`, `removing`), `package`, `ref`, `installation`, `current`, `total` | before each operation |
| `finished` | `success`, `changed`, `summary`, `skipped` | last record |

With `--after TX` it follows that kyu-helper transaction to its end. It skips the Flatpaks (`skipped: true`, exit code 2) when the pacman update was cancelled, timed out, or stopped at a question Kyu does not recognise. Otherwise it updates them even if pacman failed: the two are independent. Each Flatpak ref is deployed atomically, so stopping kyu-flatpak (at logout, say) leaves every app on either its old or its new version. kyu-flatpak does not add remotes or switch end-of-life apps to their replacements; it logs both. An unreadable remote counts as a failure. libflatpak itself treats one as "no updates".

## Lifetime

- SIGHUP is ignored.
- SIGTERM or SIGINT while pacman runs: the helper waits for pacman to finish, then exits. The unit has `KillMode=mixed` and `TimeoutStopSec=30min`, so systemd sends SIGTERM to the helper only.
- During a transaction the helper holds a logind `block` inhibitor for shutdown and sleep, if logind is available.
- A pending question does not count as idle; a question nobody answers ends after `--question-timeout` (default 30 minutes).
- The helper exits after 5 minutes without a transaction. The next call starts it again through D-Bus activation.
