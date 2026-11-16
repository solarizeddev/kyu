//! Shared state of the helper: the running or last transaction, and the one
//! place that numbers, logs and broadcasts events.

use std::path::PathBuf;
use std::sync::{Arc, Mutex, OnceLock};
use std::time::{Duration, Instant};

use serde_json::{Map, Value, json};
use tokio::sync::oneshot;
use zbus::Connection;
use zbus::object_server::SignalEmitter;

use crate::OBJECT_PATH;
use crate::log::{self, FORMAT_VERSION, TransactionLog};
use crate::service::Helper;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Phase {
    Idle,
    Running,
    Finished,
    Failed,
}

impl Phase {
    fn as_str(self) -> &'static str {
        match self {
            Phase::Idle => "idle",
            Phase::Running => "running",
            Phase::Finished => "finished",
            Phase::Failed => "failed",
        }
    }
}

/// What the user decided about a relayed question.
pub enum Reply {
    Answer { value: String, uid: u32 },
    Cancel { uid: u32 },
}

#[derive(Debug, PartialEq, Eq)]
pub enum ReplyError {
    /// No such transaction or question, or it was already answered.
    NoQuestion(String),
    /// Caller is not the user who started the update (or root).
    NotAllowed(String),
    /// Not one of pacman's options.
    InvalidAnswer(String),
}

struct PendingQuestion {
    id: u64,
    values: Vec<String>,
    sender: oneshot::Sender<Reply>,
}

#[derive(Debug)]
pub enum BeginError {
    Busy(String),
    Failed(String),
}

pub struct Config {
    /// Session-bus mode for development and tests: no polkit, no logind
    /// inhibitor, and the paths below may be overridden.
    pub session: bool,
    pub pacman: PathBuf,
    pub db_lock: PathBuf,
    pub log_dir: PathBuf,
    /// How long pacman may wait for an answer before the helper stops it.
    pub question_timeout: Duration,
    /// Where restart detection looks (/proc, /usr/lib/modules).
    pub roots: crate::aftercare::Roots,
}

#[derive(Default)]
struct Transaction {
    id: String,
    uid: u32,
    started_at: String,
    log_path: PathBuf,
    stage: String,
    current: u32,
    total: u32,
    action: String,
    package: String,
    summary: String,
    question: Value,
}

struct Inner {
    phase: Phase,
    seq: u64,
    tx: Option<Transaction>,
    log: Option<TransactionLog>,
    /// Set when the helper starts and finds an earlier log.
    previous: Option<(PathBuf, Map<String, Value>)>,
    last_activity: Instant,
    exit_when_done: bool,
    pending: Option<PendingQuestion>,
}

pub struct Shared {
    pub config: Config,
    inner: Mutex<Inner>,
    conn: OnceLock<Connection>,
}

impl Shared {
    pub fn new(config: Config) -> Arc<Self> {
        let previous = log::newest(&config.log_dir);
        Arc::new(Self {
            config,
            inner: Mutex::new(Inner {
                phase: Phase::Idle,
                seq: 0,
                tx: None,
                log: None,
                previous,
                last_activity: Instant::now(),
                exit_when_done: false,
                pending: None,
            }),
            conn: OnceLock::new(),
        })
    }

    pub fn set_connection(&self, conn: Connection) {
        let _ = self.conn.set(conn);
    }

    fn lock(&self) -> std::sync::MutexGuard<'_, Inner> {
        // A panic while holding the lock must not take the D-Bus API down
        // with it; the data is still consistent enough to report.
        self.inner.lock().unwrap_or_else(|e| e.into_inner())
    }

    pub fn touch(&self) {
        self.lock().last_activity = Instant::now();
    }

    pub fn idle_for(&self) -> Option<std::time::Duration> {
        let inner = self.lock();
        (inner.phase != Phase::Running).then(|| inner.last_activity.elapsed())
    }

    pub fn is_running(&self) -> bool {
        self.lock().phase == Phase::Running
    }

    /// Returns true if the process should exit now; otherwise exits once the
    /// running transaction has finished.
    pub fn request_exit(&self) -> bool {
        let mut inner = self.lock();
        if inner.phase == Phase::Running {
            inner.exit_when_done = true;
            false
        } else {
            true
        }
    }

    /// Claims the helper for a new transaction. Fails if one is running.
    pub fn begin(&self, uid: u32) -> Result<String, BeginError> {
        let mut inner = self.lock();
        if inner.phase == Phase::Running {
            let tx = inner
                .tx
                .as_ref()
                .map(|t| (t.id.clone(), t.uid))
                .unwrap_or_default();
            return Err(BeginError::Busy(format!(
                "an update is already running (transaction {}, started by uid {})",
                tx.0, tx.1
            )));
        }
        // Ids have one-second resolution; the counter separates updates
        // started within the same second (also across helper restarts).
        let mut attempt = 0;
        let (id, log) = loop {
            let id = log::transaction_id(attempt);
            match TransactionLog::create(&self.config.log_dir, &id) {
                Ok(log) => break (id, log),
                Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists && attempt < 100 => {
                    attempt += 1
                }
                Err(e) => {
                    return Err(BeginError::Failed(format!(
                        "cannot create the transaction log in {}: {e}",
                        self.config.log_dir.display()
                    )));
                }
            }
        };
        inner.tx = Some(Transaction {
            id: id.clone(),
            uid,
            started_at: log::now_rfc3339(),
            log_path: log.path().to_path_buf(),
            ..Default::default()
        });
        inner.log = Some(log);
        inner.seq = 0;
        inner.phase = Phase::Running;
        inner.previous = None;
        inner.last_activity = Instant::now();
        Ok(id)
    }

    /// Numbers the event, appends it to the log, updates the state and
    /// broadcasts it. Only the transaction task calls this, so events go out
    /// in order.
    pub async fn emit(&self, kind: &str, fields: Value) {
        let (tx_id, seq, record) = {
            let mut inner = self.lock();
            inner.seq += 1;
            let seq = inner.seq;
            let success = fields.get("success").and_then(Value::as_bool) == Some(true);
            let Some(tx) = inner.tx.as_mut() else { return };
            apply(tx, kind, &fields);
            let mut record = Map::new();
            record.insert("v".into(), json!(FORMAT_VERSION));
            record.insert("tx".into(), json!(tx.id));
            record.insert("seq".into(), json!(seq));
            record.insert("time".into(), json!(log::now_rfc3339()));
            record.insert("kind".into(), json!(kind));
            if let Value::Object(extra) = fields {
                record.extend(extra);
            }
            let tx_id = tx.id.clone();
            let line = Value::Object(record).to_string();
            if let Some(log) = inner.log.as_mut()
                && let Err(e) = log.append(&line)
            {
                eprintln!("kyu-helper: cannot write the transaction log: {e}");
            }
            if kind == "finished" {
                inner.phase = if success {
                    Phase::Finished
                } else {
                    Phase::Failed
                };
                inner.log = None;
                inner.last_activity = Instant::now();
            }
            (tx_id, seq, line)
        };
        if let Some(conn) = self.conn.get() {
            match SignalEmitter::new(conn, OBJECT_PATH) {
                Ok(emitter) => {
                    if let Err(e) = Helper::event(&emitter, &tx_id, seq, &record).await {
                        eprintln!("kyu-helper: cannot send event signal: {e}");
                    }
                }
                Err(e) => eprintln!("kyu-helper: {e}"),
            }
        }
    }

    /// Registers a relayed question, then announces it. The answer (or a
    /// cancel) arrives on the returned channel.
    pub async fn wait_for_reply(
        &self,
        id: u64,
        values: Vec<String>,
        record: Value,
    ) -> oneshot::Receiver<Reply> {
        let (sender, receiver) = oneshot::channel();
        self.lock().pending = Some(PendingQuestion { id, values, sender });
        self.emit("question", record).await;
        receiver
    }

    pub fn clear_question(&self) {
        self.lock().pending = None;
    }

    /// Answer(transaction, question, answer) from D-Bus.
    pub fn answer(&self, tx: &str, id: u64, uid: u32, value: &str) -> Result<(), ReplyError> {
        let mut inner = self.lock();
        Self::check_reply(&inner, tx, Some(id), uid)?;
        let pending = inner.pending.as_ref().expect("checked");
        if !pending.values.iter().any(|v| v == value) {
            return Err(ReplyError::InvalidAnswer(format!(
                "\"{value}\" is not one of pacman's answers ({})",
                pending.values.join(", ")
            )));
        }
        let pending = inner.pending.take().expect("checked");
        inner.last_activity = Instant::now();
        let _ = pending.sender.send(Reply::Answer {
            value: value.to_string(),
            uid,
        });
        Ok(())
    }

    /// Cancel(transaction) from D-Bus: only while pacman waits for an answer,
    /// which is before it changes any package.
    pub fn cancel(&self, tx: &str, uid: u32) -> Result<(), ReplyError> {
        let mut inner = self.lock();
        Self::check_reply(&inner, tx, None, uid)?;
        let pending = inner.pending.take().expect("checked");
        let _ = pending.sender.send(Reply::Cancel { uid });
        Ok(())
    }

    fn check_reply(inner: &Inner, tx: &str, id: Option<u64>, uid: u32) -> Result<(), ReplyError> {
        let current = inner
            .tx
            .as_ref()
            .filter(|t| t.id == tx && inner.phase == Phase::Running)
            .ok_or_else(|| ReplyError::NoQuestion(format!("transaction {tx} is not running")))?;
        if uid != current.uid && uid != 0 {
            return Err(ReplyError::NotAllowed(format!(
                "only the user who started the update (uid {}) can answer or cancel it",
                current.uid
            )));
        }
        match &inner.pending {
            Some(p) if id.is_none_or(|id| id == p.id) => Ok(()),
            Some(p) => Err(ReplyError::NoQuestion(format!("pacman is waiting on question {}, not {}", p.id, id.unwrap_or(0)))),
            None => Err(ReplyError::NoQuestion(
                "pacman is not waiting for an answer. Kyu stops pacman only at a question, before it changes packages".into(),
            )),
        }
    }

    pub fn exit_when_done(&self) -> bool {
        self.lock().exit_when_done
    }

    /// GetState payload. Same vocabulary as the log records.
    pub fn state_json(&self) -> String {
        let inner = self.lock();
        let mut state = json!({
            "v": FORMAT_VERSION,
            "phase": inner.phase.as_str(),
            "seq": inner.seq,
        });
        if let Some(tx) = &inner.tx {
            let obj = state.as_object_mut().expect("object");
            obj.insert(
                "transaction".into(),
                json!({
                    "tx": tx.id,
                    "uid": tx.uid,
                    "startedAt": tx.started_at,
                    "log": tx.log_path,
                    "stage": tx.stage,
                    "current": tx.current,
                    "total": tx.total,
                    "action": tx.action,
                    "package": tx.package,
                    "summary": tx.summary,
                    "question": tx.question,
                }),
            );
        } else if let Some((path, last)) = &inner.previous {
            // The helper restarted since the last transaction. If that log
            // does not end with "finished", the transaction was interrupted.
            let obj = state.as_object_mut().expect("object");
            let finished = last.get("kind").and_then(Value::as_str) == Some("finished");
            obj.insert(
                "previous".into(),
                json!({
                    "tx": last.get("tx").cloned().unwrap_or(Value::Null),
                    "log": path,
                    "interrupted": !finished,
                    "success": finished && last.get("success").and_then(Value::as_bool) == Some(true),
                    "summary": last.get("summary").cloned().unwrap_or(Value::Null),
                }),
            );
        }
        state.to_string()
    }
}

fn apply(tx: &mut Transaction, kind: &str, fields: &Value) {
    let s = |key: &str| {
        fields
            .get(key)
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_string()
    };
    let n = |key: &str| fields.get(key).and_then(Value::as_u64).unwrap_or_default() as u32;
    match kind {
        "stage" => tx.stage = s("stage"),
        "packages" => tx.total = n("total"),
        "progress" => {
            tx.current = n("current");
            tx.action = s("action");
            tx.package = s("package");
        }
        "question" => tx.question = fields.clone(),
        "answered" | "cancelled" | "timeout" => tx.question = Value::Null,
        "finished" => {
            tx.summary = s("summary");
            tx.question = Value::Null;
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn shared(name: &str) -> Arc<Shared> {
        let dir =
            std::env::temp_dir().join(format!("kyu-state-test-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        Shared::new(Config {
            session: true,
            pacman: PathBuf::from("/bin/false"),
            db_lock: dir.join("db.lck"),
            log_dir: dir.clone(),
            question_timeout: Duration::from_secs(60),
            roots: crate::aftercare::Roots {
                proc_dir: dir.join("proc"),
                modules_dir: dir.join("modules"),
            },
        })
    }

    #[tokio::test]
    async fn answers_are_checked() {
        let s = shared("answers");
        let tx = s.begin(1000).unwrap();
        // Nothing asked yet.
        assert!(matches!(
            s.answer(&tx, 1, 1000, "y"),
            Err(ReplyError::NoQuestion(_))
        ));
        assert!(matches!(
            s.cancel(&tx, 1000),
            Err(ReplyError::NoQuestion(_))
        ));

        let mut reply = s
            .wait_for_reply(1, vec!["1".into(), "2".into()], json!({}))
            .await;
        assert!(matches!(
            s.answer("other-tx", 1, 1000, "2"),
            Err(ReplyError::NoQuestion(_))
        ));
        assert!(matches!(
            s.answer(&tx, 2, 1000, "2"),
            Err(ReplyError::NoQuestion(_))
        ));
        assert!(matches!(
            s.answer(&tx, 1, 1001, "2"),
            Err(ReplyError::NotAllowed(_))
        ));
        assert!(matches!(
            s.answer(&tx, 1, 1000, "3"),
            Err(ReplyError::InvalidAnswer(_))
        ));
        assert!(
            reply.try_recv().is_err(),
            "rejected answers must not reach pacman"
        );

        s.answer(&tx, 1, 1000, "2").unwrap();
        assert!(
            matches!(reply.try_recv(), Ok(Reply::Answer { ref value, uid: 1000 }) if value == "2")
        );
        // Answered once only.
        assert!(matches!(
            s.answer(&tx, 1, 1000, "2"),
            Err(ReplyError::NoQuestion(_))
        ));
    }

    #[tokio::test]
    async fn root_may_cancel() {
        let s = shared("cancel");
        let tx = s.begin(1000).unwrap();
        let mut reply = s
            .wait_for_reply(1, vec!["y".into(), "n".into()], json!({}))
            .await;
        assert!(matches!(
            s.cancel(&tx, 1001),
            Err(ReplyError::NotAllowed(_))
        ));
        s.cancel(&tx, 0).unwrap();
        assert!(matches!(reply.try_recv(), Ok(Reply::Cancel { uid: 0 })));
    }
}
