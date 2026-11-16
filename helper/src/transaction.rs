//! Runs `pacman -Syu` and turns its output into events.
//!
//! pacman runs with pipes (no pty): LC_ALL=C, --noprogressbar and
//! --color never give one stable line per step. Questions arrive as an
//! unterminated line on stderr. "Proceed with installation?" is answered
//! here: the user asked for the update. Recognised questions (providers,
//! conflicts, replacements, keys, corrupted files, unresolvable packages,
//! ignored packages) are relayed to the panel, and pacman waits until the
//! user who started the update answers, cancels, or the time runs out.
//! Anything else stops pacman with SIGINT before it changes a package.

use std::collections::VecDeque;
use std::io::Read;
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::process::Stdio;
use std::sync::Arc;
use std::time::Duration;

use serde_json::json;
use tokio::io::AsyncWriteExt;
use tokio::process::Command;
use tokio::sync::mpsc;

use crate::aftercare;
use crate::failure;
use crate::parse::{self, Action, LineInfo, Prompt, Question};
use crate::state::{Reply, Shared};

/// Lines kept for question context (provider lists and the like).
const RECENT_LINES: usize = 60;
/// After a question shows up on stderr, wait this long for stdout to catch
/// up: pacman flushes stdout before asking, but the two pipes are read
/// separately.
const SETTLE: Duration = Duration::from_millis(150);

/// Arguments after the pacman binary. Fixed: callers cannot add any.
pub const PACMAN_ARGS: [&str; 4] = ["-Syu", "--noprogressbar", "--color", "never"];

/// The exact command line, as shown in the log and the panel.
pub fn command_line(pacman: &str) -> Vec<String> {
    std::iter::once(pacman.to_string())
        .chain(PACMAN_ARGS.iter().map(|a| a.to_string()))
        .collect()
}

/// pacman writes "error:" and "warning:" lines to stderr; with both streams
/// on one terminal, tell them apart by their prefix (a display hint only).
fn stream_of(text: &str) -> &'static str {
    let t = text.trim_start();
    if t.starts_with("error:") || t.starts_with("warning:") {
        "stderr"
    } else {
        "stdout"
    }
}

/// A pseudo-terminal pair for pacman's output: raw (no \r\n translation),
/// wide enough that pacman does not wrap its package lists.
fn open_pty() -> std::io::Result<(OwnedFd, OwnedFd)> {
    let mut master: libc::c_int = -1;
    let mut slave: libc::c_int = -1;
    let size = libc::winsize {
        ws_row: 50,
        ws_col: 512,
        ws_xpixel: 0,
        ws_ypixel: 0,
    };
    // SAFETY: plain libc calls on fds we own.
    unsafe {
        if libc::openpty(
            &mut master,
            &mut slave,
            std::ptr::null_mut(),
            std::ptr::null(),
            &size,
        ) != 0
        {
            return Err(std::io::Error::last_os_error());
        }
        let (master, slave) = (OwnedFd::from_raw_fd(master), OwnedFd::from_raw_fd(slave));
        libc::fcntl(master.as_raw_fd(), libc::F_SETFD, libc::FD_CLOEXEC);
        let mut termios: libc::termios = std::mem::zeroed();
        if libc::tcgetattr(slave.as_raw_fd(), &mut termios) == 0 {
            libc::cfmakeraw(&mut termios);
            libc::tcsetattr(slave.as_raw_fd(), libc::TCSANOW, &termios);
        }
        Ok((master, slave))
    }
}

#[derive(Default)]
struct Tally {
    total: u32,
    current: u32,
    done: Vec<(Action, u32)>,
    nothing_to_do: bool,
    last_error: Option<String>,
    /// Why pacman was stopped at a question, and the question.
    stopped: Option<(Stop, String)>,
    recent: VecDeque<String>,
    questions: u64,
    /// .pacnew/.pacsave files pacman reported.
    configs: Vec<serde_json::Value>,
    /// Error lines and friends, to say why an update failed.
    evidence: failure::Evidence,
    /// pacman has started changing packages (":: Processing package
    /// changes..."): no more questions, no more signals.
    committing: bool,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Stop {
    Unsupported,
    Cancelled,
    TimedOut,
}

impl Tally {
    fn count(&mut self, action: Action) {
        match self.done.iter_mut().find(|(a, _)| *a == action) {
            Some((_, n)) => *n += 1,
            None => self.done.push((action, 1)),
        }
    }

    fn done_text(&self) -> String {
        self.done
            .iter()
            .map(|(a, n)| format!("{n} {}", a.done_str()))
            .collect::<Vec<_>>()
            .join(", ")
    }
}

/// Runs one transaction to the end. `_inhibitor` is a logind inhibitor
/// lock held until this returns, so the machine does not shut down or sleep
/// mid-update.
pub async fn run(shared: Arc<Shared>, uid: u32, _inhibitor: Option<OwnedFd>) {
    let pacman = shared.config.pacman.display().to_string();
    let started_failed = |e: String| {
        let shared = shared.clone();
        let pacman = pacman.clone();
        async move {
            shared
                .emit(
                    "started",
                    json!({ "uid": uid, "command": command_line(&pacman) }),
                )
                .await;
            shared.emit("finished", json!({ "success": false, "summary": format!("Could not start {pacman}: {e}") })).await;
        }
    };
    // pacman's stdout and stderr go to one pseudo-terminal: pacman then
    // line-buffers stdout (a pipe would get 4 KiB blocks, so progress and a
    // provider list could arrive late), both streams arrive in the order
    // pacman wrote them, and if the helper dies pacman's writes fail with
    // EIO instead of a SIGPIPE that would kill it mid-commit.
    let (master, slave) = match open_pty() {
        Ok(fds) => fds,
        Err(e) => return started_failed(format!("cannot open a pseudo-terminal: {e}")).await,
    };
    let slave_err = match slave.try_clone() {
        Ok(fd) => fd,
        Err(e) => return started_failed(format!("cannot open a pseudo-terminal: {e}")).await,
    };
    let mut command = Command::new(&shared.config.pacman);
    command
        .args(PACMAN_ARGS)
        .env_clear()
        .env(
            "PATH",
            "/usr/local/sbin:/usr/local/bin:/usr/bin:/usr/sbin:/bin:/sbin",
        )
        .env("LC_ALL", "C")
        .stdin(Stdio::piped())
        .stdout(Stdio::from(slave))
        .stderr(Stdio::from(slave_err))
        // Never kill pacman because the helper's handle went away.
        .kill_on_drop(false);
    // SAFETY: async-signal-safe calls only, between fork and exec.
    unsafe {
        command.pre_exec(|| {
            // stdin is a pipe from the helper: if the helper dies while pacman
            // reads an answer, a write must not kill pacman either.
            libc::signal(libc::SIGPIPE, libc::SIG_IGN);
            Ok(())
        });
    }

    let spawned = command.spawn();
    // The parent's copies of the terminal must close, or reading never ends.
    drop(command);
    let mut child = match spawned {
        Ok(child) => child,
        Err(e) => return started_failed(e.to_string()).await,
    };
    let pid = child.id().unwrap_or_default();
    shared
        .emit(
            "started",
            json!({ "uid": uid, "command": command_line(&pacman), "pid": pid }),
        )
        .await;

    let (sender, mut chunks) = mpsc::channel::<Vec<u8>>(64);
    std::thread::spawn(move || {
        let mut master = std::fs::File::from(master);
        let mut buf = vec![0u8; 8192];
        loop {
            // EIO once every writer (pacman, its hooks) has closed the terminal.
            match master.read(&mut buf) {
                Ok(0) | Err(_) => break,
                Ok(n) => {
                    if sender.blocking_send(buf[..n].to_vec()).is_err() {
                        break;
                    }
                }
            }
        }
    });

    let mut stdin = child.stdin.take();
    let mut tally = Tally::default();
    let mut pending = Vec::<u8>::new();

    while let Some(bytes) = chunks.recv().await {
        ingest(&shared, &mut tally, &mut pending, &bytes).await;
        // pacman asks nothing once it has started changing packages; from
        // then on an unterminated line is just output (a hook's, say), and
        // the helper never writes to pacman or signals it.
        if tally.committing {
            continue;
        }
        let tail = String::from_utf8_lossy(&pending).to_string();
        if parse::prompt(&tail, &[]).is_none() {
            continue;
        }
        // A question is followed by silence: pacman waits. More output means
        // this was only the end of a chunk.
        if let Ok(Some(more)) = tokio::time::timeout(SETTLE, chunks.recv()).await {
            ingest(&shared, &mut tally, &mut pending, &more).await;
            continue;
        }
        let recent: Vec<String> = tally.recent.iter().cloned().collect();
        let Some(prompt) = parse::prompt(&tail, &recent) else {
            continue;
        };
        pending.clear();
        shared
            .emit(
                "output",
                json!({ "stream": stream_of(&tail), "text": tail }),
            )
            .await;
        let answer = match prompt {
            Prompt::Proceed => {
                shared
                    .emit(
                        "answered",
                        json!({ "question": tail.trim(), "answer": "y", "reason": "Update all was chosen" }),
                    )
                    .await;
                Some("y".to_string())
            }
            Prompt::Unsupported => {
                shared
                    .emit("question", json!({ "text": tail.trim(), "handled": false }))
                    .await;
                tally.stopped = Some((Stop::Unsupported, tail.trim().to_string()));
                None
            }
            Prompt::Relay(question) => ask(&shared, &mut tally, question).await,
        };
        match answer {
            Some(answer) => {
                let written = match stdin.as_mut() {
                    Some(stdin) => stdin
                        .write_all(format!("{answer}\n").as_bytes())
                        .await
                        .and(stdin.flush().await)
                        .is_ok(),
                    None => false,
                };
                if !written {
                    eprintln!("kyu-helper: could not answer pacman's question");
                }
            }
            None => {
                // pacman is blocked reading stdin, before any package is
                // changed (all its questions come before that). SIGINT
                // makes it clean up and exit.
                if pid != 0 {
                    // SAFETY: plain kill(2) on our own child.
                    unsafe { libc::kill(pid as i32, libc::SIGINT) };
                }
            }
        }
    }
    if !pending.is_empty() {
        let text = String::from_utf8_lossy(&pending).to_string();
        handle_line(&shared, &mut tally, text).await;
    }
    drop(stdin);

    let status = child.wait().await;
    let (success, exit_code, summary) =
        summarize(&tally, &status, shared.config.question_timeout.as_secs());
    let changed: u32 = tally.done.iter().map(|(_, n)| *n).sum();
    shared
        .emit(
            "finished",
            json!({
                "success": success,
                "exitCode": exit_code,
                "summary": summary,
                "changed": changed,
                // Settings files pacman did not overwrite (.pacnew) or kept
                // from removed packages (.pacsave).
                "configs": tally.configs,
                // Why it failed, from pacman's own lines (null on success or
                // when the helper stopped pacman at a question).
                "failure": if success || tally.stopped.is_some() { serde_json::Value::Null } else { tally.evidence.classify().unwrap_or(serde_json::Value::Null) },
                // Only when packages changed: what still runs old code.
                "restart": if changed > 0 { aftercare::restart(&shared.config.roots, uid) } else { serde_json::Value::Null },
            }),
        )
        .await;
}

/// Appends a chunk to its stream's buffer and handles every complete line.
async fn ingest(shared: &Shared, tally: &mut Tally, pending: &mut Vec<u8>, bytes: &[u8]) {
    pending.extend_from_slice(bytes);
    while let Some(end) = pending.iter().position(|&b| b == b'\n') {
        let line: Vec<u8> = pending.drain(..=end).collect();
        let text = String::from_utf8_lossy(&line[..line.len() - 1])
            .trim_end_matches('\r')
            .to_string();
        handle_line(shared, tally, text).await;
    }
}

/// Relays a question and waits for the answer. Returns what to write to
/// pacman, or None to stop it.
async fn ask(shared: &Shared, tally: &mut Tally, question: Question) -> Option<String> {
    tally.questions += 1;
    let id = tally.questions;
    let values: Vec<String> = question.answers.iter().map(|a| a.value.clone()).collect();
    let details: serde_json::Map<String, serde_json::Value> = question
        .details
        .iter()
        .map(|(k, v)| (k.to_string(), json!(v)))
        .collect();
    let timeout = shared.config.question_timeout;
    let record = json!({
        "id": id,
        "handled": true,
        "type": question.kind,
        "text": question.prompt,
        "context": question.context,
        "answers": question.answers.iter().map(|a| json!({ "value": a.value, "label": a.label, "detail": a.detail })).collect::<Vec<_>>(),
        "default": question.default,
        "details": details,
        "timeoutSeconds": timeout.as_secs(),
    });
    let reply = shared.wait_for_reply(id, values, record).await;
    match tokio::time::timeout(timeout, reply).await {
        Ok(Ok(Reply::Answer { value, uid })) => {
            shared
                .emit(
                    "answered",
                    json!({ "id": id, "question": question.prompt, "answer": value, "uid": uid }),
                )
                .await;
            Some(value)
        }
        Ok(Ok(Reply::Cancel { uid })) => {
            shared
                .emit("cancelled", json!({ "id": id, "uid": uid }))
                .await;
            tally.stopped = Some((Stop::Cancelled, question.prompt));
            None
        }
        Ok(Err(_)) | Err(_) => {
            shared.clear_question();
            shared
                .emit("timeout", json!({ "id": id, "seconds": timeout.as_secs() }))
                .await;
            tally.stopped = Some((Stop::TimedOut, question.prompt));
            None
        }
    }
}

async fn handle_line(shared: &Shared, tally: &mut Tally, text: String) {
    if tally.recent.len() == RECENT_LINES {
        tally.recent.pop_front();
    }
    tally.recent.push_back(text.clone());
    tally.evidence.add(&text);
    shared
        .emit(
            "output",
            json!({ "stream": stream_of(&text), "text": text }),
        )
        .await;
    match parse::classify(&text) {
        LineInfo::PackageCount(total) => {
            tally.total = total;
            shared.emit("packages", json!({ "total": total })).await;
        }
        // pacman prints one line per package only while changing packages;
        // the same words from anything else are not operations.
        LineInfo::Operation { action, package } if tally.committing => {
            tally.current += 1;
            tally.count(action);
            shared
                .emit(
                    "progress",
                    json!({
                        "action": action.as_str(),
                        "package": package,
                        "current": tally.current,
                        "total": tally.total.max(tally.current),
                    }),
                )
                .await;
        }
        LineInfo::Stage(stage) => {
            if matches!(stage, parse::Stage::Commit | parse::Stage::Hooks) {
                tally.committing = true;
            }
            shared
                .emit("stage", json!({ "stage": stage.as_str() }))
                .await
        }
        LineInfo::NothingToDo => tally.nothing_to_do = true,
        LineInfo::Error(message) => tally.last_error = Some(message),
        LineInfo::Config { kind, path, saved } => tally
            .configs
            .push(json!({ "kind": kind, "path": path, "saved": saved })),
        LineInfo::Operation { .. } | LineInfo::Other => {}
    }
}

fn summarize(
    tally: &Tally,
    status: &std::io::Result<std::process::ExitStatus>,
    timeout_seconds: u64,
) -> (bool, Option<i32>, String) {
    let changed: u32 = tally.done.iter().map(|(_, n)| *n).sum();
    let exit_code = status.as_ref().ok().and_then(|s| s.code());
    let ok = exit_code == Some(0);

    if let Some((why, question)) = &tally.stopped {
        let mut summary = match why {
            Stop::Unsupported => {
                format!("Stopped at a question Kyu does not recognise: \"{question}\"")
            }
            Stop::Cancelled => format!("Cancelled at pacman's question \"{question}\""),
            Stop::TimedOut => format!(
                "No answer to \"{question}\" within {}, so Kyu stopped pacman",
                if timeout_seconds < 120 {
                    format!("{timeout_seconds} seconds")
                } else {
                    format!("{} minutes", timeout_seconds.div_ceil(60))
                }
            ),
        };
        if changed == 0 {
            summary.push_str(". No packages were changed.");
        } else {
            summary.push_str(&format!(". {} before it.", tally.done_text()));
        }
        return (false, exit_code, summary);
    }
    if ok {
        let summary = if tally.nothing_to_do || changed == 0 {
            "Nothing to do".to_string()
        } else {
            tally.done_text()
        };
        return (true, exit_code, summary);
    }
    let reason = match (&tally.last_error, status) {
        (Some(error), _) => format!("pacman: {error}"),
        (None, Ok(s)) => match s.code() {
            Some(code) => format!("pacman exited with code {code}"),
            None => format!("pacman was stopped by a signal ({s})"),
        },
        (None, Err(e)) => format!("could not wait for pacman: {e}"),
    };
    let summary = if changed == 0 {
        format!("{reason}. No packages were changed.")
    } else {
        format!("{reason}. {} before the error.", tally.done_text())
    };
    (false, exit_code, summary)
}
