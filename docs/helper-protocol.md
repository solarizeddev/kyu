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
    "summary": "", "question": ""
  }
}
```

- `phase` is one of `idle`, `running`, `finished` or `failed`.
- `transaction` is the current or last transaction since the helper started.
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
| `answered` | `question`, `answer`, `reason` | the helper answered a question (only "Proceed with installation?") |
| `question` | `text`, `handled: false` | a question the helper cannot answer yet; it stops pacman with SIGINT |
| `finished` | `success`, `exitCode`, `summary`, `changed` | last record |

`summary` states what happened. Examples:
- `3 upgraded`
- `Nothing to do`
- `pacman: failed to commit transaction (conflicting files). 2 upgraded before the error.`
- `Stopped at a question Kyu cannot answer yet: "…". No packages were changed.`

"No packages were changed" is stated only when no package operation was seen in pacman's output.

## Following a transaction without gaps

1. Subscribe to `Event`.
2. Call `GetState`. If it has a `transaction`, read its `log` file, delivering records in `seq` order.
3. Deliver signals with a `seq` above the last one delivered. On a gap, read the log again.
4. Stop at `finished`. If the service leaves the bus first, the helper died. Report it; the log ends without a result.

`HelperClient` (`src/client/helperclient.cpp`) implements this.

## Questions (milestone 2 behaviour)

pacman prints questions on stderr without a newline. The helper answers only `:: Proceed with installation? [Y/n]`, with `y`, because the user chose "Update all". Any other question (providers, conflicts, replacements, key imports, corrupted packages) is logged as `question`, and pacman gets SIGINT while it waits for input, before it changes any package. Relaying questions to the panel is milestone 4.

## Lifetime

- SIGHUP is ignored.
- SIGTERM or SIGINT while pacman runs: the helper waits for pacman to finish, then exits. The unit has `KillMode=mixed` and `TimeoutStopSec=30min`, so systemd sends SIGTERM to the helper only.
- During a transaction the helper holds a logind `block` inhibitor for shutdown and sleep, if logind is available.
- The helper exits after 5 minutes without a transaction. The next call starts it again through D-Bus activation.
