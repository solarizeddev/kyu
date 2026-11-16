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
use std::os::fd::OwnedFd;
use std::process::Stdio;
use std::sync::Arc;
use std::time::Duration;

use serde_json::json;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::process::Command;
use tokio::sync::mpsc;

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

#[derive(Clone, Copy)]
enum Stream {
    Stdout,
    Stderr,
}

impl Stream {
    fn as_str(self) -> &'static str {
        match self {
            Stream::Stdout => "stdout",
            Stream::Stderr => "stderr",
        }
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
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        // Never kill pacman because the helper's handle went away.
        .kill_on_drop(false);

    let mut child = match command.spawn() {
        Ok(child) => child,
        Err(e) => {
            shared
                .emit(
                    "started",
                    json!({ "uid": uid, "command": command_line(&pacman) }),
                )
                .await;
            shared
                .emit("finished", json!({ "success": false, "summary": format!("Could not start {pacman}: {e}") }))
                .await;
            return;
        }
    };
    let pid = child.id().unwrap_or_default();
    shared
        .emit(
            "started",
            json!({ "uid": uid, "command": command_line(&pacman), "pid": pid }),
        )
        .await;

    let (sender, mut chunks) = mpsc::channel::<(Stream, Vec<u8>)>(64);
    for (stream, reader) in [
        (
            Stream::Stdout,
            child
                .stdout
                .take()
                .map(|r| Box::new(r) as Box<dyn tokio::io::AsyncRead + Unpin + Send>),
        ),
        (
            Stream::Stderr,
            child
                .stderr
                .take()
                .map(|r| Box::new(r) as Box<dyn tokio::io::AsyncRead + Unpin + Send>),
        ),
    ] {
        let Some(mut reader) = reader else { continue };
        let sender = sender.clone();
        tokio::spawn(async move {
            let mut buf = vec![0u8; 8192];
            loop {
                match reader.read(&mut buf).await {
                    Ok(0) | Err(_) => break,
                    Ok(n) => {
                        if sender.send((stream, buf[..n].to_vec())).await.is_err() {
                            break;
                        }
                    }
                }
            }
        });
    }
    drop(sender);

    let mut stdin = child.stdin.take();
    let mut tally = Tally::default();
    let mut pending = [Vec::<u8>::new(), Vec::<u8>::new()];

    while let Some((stream, bytes)) = chunks.recv().await {
        ingest(&shared, &mut tally, &mut pending, stream, &bytes).await;
        for stream in [Stream::Stdout, Stream::Stderr] {
            let tail = String::from_utf8_lossy(&pending[stream as usize]).to_string();
            if parse::prompt(&tail, &[]).is_none() {
                continue;
            }
            // Let the other pipe catch up so the question's context is complete.
            while let Ok(Some((s, b))) = tokio::time::timeout(SETTLE, chunks.recv()).await {
                ingest(&shared, &mut tally, &mut pending, s, &b).await;
            }
            let tail = String::from_utf8_lossy(&pending[stream as usize]).to_string();
            let recent: Vec<String> = tally.recent.iter().cloned().collect();
            let Some(prompt) = parse::prompt(&tail, &recent) else {
                continue;
            };
            pending[stream as usize].clear();
            shared
                .emit("output", json!({ "stream": stream.as_str(), "text": tail }))
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
    }
    for (index, buffer) in pending.iter().enumerate() {
        if !buffer.is_empty() {
            let stream = if index == 0 {
                Stream::Stdout
            } else {
                Stream::Stderr
            };
            let text = String::from_utf8_lossy(buffer).to_string();
            handle_line(&shared, &mut tally, stream, text).await;
        }
    }
    drop(stdin);

    let status = child.wait().await;
    let (success, exit_code, summary) =
        summarize(&tally, &status, shared.config.question_timeout.as_secs());
    shared
        .emit(
            "finished",
            json!({
                "success": success,
                "exitCode": exit_code,
                "summary": summary,
                "changed": tally.done.iter().map(|(_, n)| *n).sum::<u32>(),
            }),
        )
        .await;
}

/// Appends a chunk to its stream's buffer and handles every complete line.
async fn ingest(
    shared: &Shared,
    tally: &mut Tally,
    pending: &mut [Vec<u8>; 2],
    stream: Stream,
    bytes: &[u8],
) {
    let buffer = &mut pending[stream as usize];
    buffer.extend_from_slice(bytes);
    while let Some(end) = buffer.iter().position(|&b| b == b'\n') {
        let line: Vec<u8> = buffer.drain(..=end).collect();
        let text = String::from_utf8_lossy(&line[..line.len() - 1])
            .trim_end_matches('\r')
            .to_string();
        handle_line(shared, tally, stream, text).await;
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

async fn handle_line(shared: &Shared, tally: &mut Tally, stream: Stream, text: String) {
    if tally.recent.len() == RECENT_LINES {
        tally.recent.pop_front();
    }
    tally.recent.push_back(text.clone());
    shared
        .emit("output", json!({ "stream": stream.as_str(), "text": text }))
        .await;
    match parse::classify(&text) {
        LineInfo::PackageCount(total) => {
            tally.total = total;
            shared.emit("packages", json!({ "total": total })).await;
        }
        LineInfo::Operation { action, package } => {
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
            shared
                .emit("stage", json!({ "stage": stage.as_str() }))
                .await
        }
        LineInfo::NothingToDo => tally.nothing_to_do = true,
        LineInfo::Error(message) => tally.last_error = Some(message),
        LineInfo::Other => {}
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
