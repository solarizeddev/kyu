//! kyu-helper: the only part of Kyu that runs as root.
//!
//! Started by D-Bus activation on the system bus (via kyu-helper.service).
//! Owns each pacman transaction from start to end, independent of any panel:
//! a panel can close, crash or reattach without affecting the update.

mod log;
mod parse;
mod service;
mod state;
mod transaction;

use std::path::PathBuf;
use std::process::ExitCode;
use std::time::Duration;

use tokio::signal::unix::{SignalKind, signal};

use service::Helper;
use state::{Config, Shared};

pub const BUS_NAME: &str = "io.github.solarizeddev.kyu.Helper";
pub const OBJECT_PATH: &str = "/io/github/solarizeddev/kyu/Helper";
pub const POLKIT_ACTION: &str = "io.github.solarizeddev.kyu.update";

const IDLE_EXIT: Duration = Duration::from_secs(5 * 60);
const QUESTION_TIMEOUT: Duration = Duration::from_secs(30 * 60);

const USAGE: &str = "\
Usage: kyu-helper [--session [--pacman PATH] [--db-lock PATH] [--log-dir PATH]] [--idle-exit SECONDS]
                  [--question-timeout SECONDS]

Runs on the system bus as root, started by D-Bus activation.

  --session         Development mode: own the name on the session bus, run as
                    the calling user, skip polkit and the shutdown inhibitor.
  --pacman PATH     pacman binary (session mode only)
  --db-lock PATH    pacman database lock to check (session mode only)
  --log-dir PATH    transaction log directory (session mode only)
  --idle-exit SECS  exit after this long without a transaction (default 300)
  --question-timeout SECS
                    stop pacman if a question is not answered in time
                    (default 1800)
";

fn parse_args() -> Result<(Config, Duration), String> {
    let mut session = false;
    let mut pacman = None;
    let mut db_lock = None;
    let mut log_dir = None;
    let mut idle = IDLE_EXIT;
    let mut question_timeout = QUESTION_TIMEOUT;
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = |name: &str| args.next().ok_or_else(|| format!("{name} needs a value"));
        match arg.as_str() {
            "--session" => session = true,
            "--pacman" => pacman = Some(PathBuf::from(value("--pacman")?)),
            "--db-lock" => db_lock = Some(PathBuf::from(value("--db-lock")?)),
            "--log-dir" => log_dir = Some(PathBuf::from(value("--log-dir")?)),
            "--idle-exit" => {
                let secs = value("--idle-exit")?;
                idle = Duration::from_secs(
                    secs.parse()
                        .map_err(|_| format!("bad --idle-exit value: {secs}"))?,
                );
            }
            "--question-timeout" => {
                let secs = value("--question-timeout")?;
                question_timeout = Duration::from_secs(
                    secs.parse()
                        .map_err(|_| format!("bad --question-timeout value: {secs}"))?,
                );
            }
            "-h" | "--help" => {
                print!("{USAGE}");
                std::process::exit(0);
            }
            other => return Err(format!("unknown argument: {other}")),
        }
    }
    // The root helper always runs the system's pacman with the system's
    // paths; overrides exist only for the unprivileged session mode.
    if !session && (pacman.is_some() || db_lock.is_some() || log_dir.is_some()) {
        return Err("--pacman, --db-lock and --log-dir need --session".into());
    }
    let db_lock = db_lock.unwrap_or_else(system_db_lock);
    Ok((
        Config {
            session,
            pacman: pacman.unwrap_or_else(|| PathBuf::from("/usr/bin/pacman")),
            db_lock,
            log_dir: log_dir.unwrap_or_else(|| PathBuf::from("/var/log/kyu")),
            question_timeout,
        },
        idle,
    ))
}

/// pacman's lock file, from pacman.conf's DBPath.
fn system_db_lock() -> PathBuf {
    let db_path = std::process::Command::new("/usr/bin/pacman-conf")
        .arg("DBPath")
        .env("LC_ALL", "C")
        .output()
        .ok()
        .filter(|o| o.status.success())
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| "/var/lib/pacman/".into());
    PathBuf::from(db_path).join("db.lck")
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> ExitCode {
    let (config, idle_exit) = match parse_args() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("kyu-helper: {e}\n\n{USAGE}");
            return ExitCode::from(2);
        }
    };
    let session = config.session;
    let shared = Shared::new(config);

    let builder = if session {
        zbus::connection::Builder::session()
    } else {
        zbus::connection::Builder::system()
    };
    let conn = match builder
        .serve_at(
            OBJECT_PATH,
            Helper {
                shared: shared.clone(),
            },
        )
        .name(BUS_NAME)
        .build()
        .await
    {
        Ok(conn) => conn,
        Err(e) => {
            eprintln!("kyu-helper: cannot own {BUS_NAME}: {e}");
            return ExitCode::FAILURE;
        }
    };
    shared.set_connection(conn.clone());

    let (mut term, mut int, mut hup) = match (
        signal(SignalKind::terminate()),
        signal(SignalKind::interrupt()),
        signal(SignalKind::hangup()),
    ) {
        (Ok(t), Ok(i), Ok(h)) => (t, i, h),
        _ => {
            eprintln!("kyu-helper: cannot install signal handlers");
            return ExitCode::FAILURE;
        }
    };
    let mut tick = tokio::time::interval(Duration::from_secs(15));
    loop {
        tokio::select! {
            _ = term.recv() => {}
            _ = int.recv() => {}
            _ = hup.recv() => {
                // Nothing to reload; above all, never stop pacman on SIGHUP.
                continue;
            }
            _ = tick.tick() => {
                if shared.idle_for().is_some_and(|idle| idle >= idle_exit) {
                    return ExitCode::SUCCESS;
                }
                continue;
            }
        }
        // SIGTERM / SIGINT: never stop pacman mid-transaction.
        if shared.request_exit() {
            return ExitCode::SUCCESS;
        }
        eprintln!("kyu-helper: stop requested; exiting when the running update has finished");
    }
}
