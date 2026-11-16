//! Runs `pacman -Syu` and turns its output into events.
//!
//! pacman runs with pipes (no pty): LC_ALL=C, --noprogressbar and
//! --color never give one stable line per step. Questions arrive as an
//! unterminated line on stderr. The only one answered here is the final
//! "Proceed with installation?": the user asked for the update. Any other
//! question stops pacman with SIGINT before it changes anything; relaying
//! questions to the panel is milestone 4.

use std::os::fd::OwnedFd;
use std::process::Stdio;
use std::sync::Arc;

use serde_json::json;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::process::Command;
use tokio::sync::mpsc;

use crate::parse::{self, Action, LineInfo, Prompt};
use crate::state::Shared;

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
    stopped_at: Option<String>,
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
        let buffer = &mut pending[stream as usize];
        buffer.extend_from_slice(&bytes);
        while let Some(end) = buffer.iter().position(|&b| b == b'\n') {
            let line: Vec<u8> = buffer.drain(..=end).collect();
            let text = String::from_utf8_lossy(&line[..line.len() - 1])
                .trim_end_matches('\r')
                .to_string();
            handle_line(&shared, &mut tally, stream, text).await;
        }
        let tail = String::from_utf8_lossy(buffer).to_string();
        match parse::prompt(&tail) {
            None => {}
            Some(Prompt::Proceed) => {
                buffer.clear();
                shared
                    .emit("output", json!({ "stream": stream.as_str(), "text": tail }))
                    .await;
                let answered = match stdin.as_mut() {
                    Some(stdin) => stdin
                        .write_all(b"y\n")
                        .await
                        .and(stdin.flush().await)
                        .is_ok(),
                    None => false,
                };
                shared
                    .emit(
                        "answered",
                        json!({ "question": tail.trim(), "answer": "y", "reason": "Update all was chosen" }),
                    )
                    .await;
                if !answered {
                    eprintln!("kyu-helper: could not answer pacman's question");
                }
            }
            Some(Prompt::Unsupported) => {
                buffer.clear();
                shared
                    .emit("output", json!({ "stream": stream.as_str(), "text": tail }))
                    .await;
                shared
                    .emit("question", json!({ "text": tail.trim(), "handled": false }))
                    .await;
                tally.stopped_at = Some(tail.trim().to_string());
                // pacman is blocked reading stdin, before any package is
                // changed. SIGINT makes it clean up and exit.
                if pid != 0 {
                    // SAFETY: plain kill(2) on our own child.
                    unsafe { libc::kill(pid as i32, libc::SIGINT) };
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
    let (success, exit_code, summary) = summarize(&tally, &status);
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

async fn handle_line(shared: &Shared, tally: &mut Tally, stream: Stream, text: String) {
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
) -> (bool, Option<i32>, String) {
    let changed: u32 = tally.done.iter().map(|(_, n)| *n).sum();
    let exit_code = status.as_ref().ok().and_then(|s| s.code());
    let ok = exit_code == Some(0);

    if let Some(question) = &tally.stopped_at {
        let mut summary = format!("Stopped at a question Kyu cannot answer yet: \"{question}\"");
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
