//! D-Bus interface io.github.solarizeddev.kyu.Helper1.
//!
//! Fixed operations only: no method takes package names, pacman arguments,
//! paths or environment from the caller.

use std::collections::HashMap;
use std::os::fd::OwnedFd;
use std::sync::Arc;

use zbus::message::Header;
use zbus::object_server::SignalEmitter;
use zbus::{Connection, fdo, interface};
use zbus_polkit::policykit1::{AuthorityProxy, CheckAuthorizationFlags, Subject};

use crate::POLKIT_ACTION;
use crate::state::{BeginError, ReplyError, Shared};
use crate::transaction;

#[derive(Debug, zbus::DBusError)]
#[zbus(prefix = "io.github.solarizeddev.kyu.Helper1.Error")]
pub enum HelperError {
    #[zbus(error)]
    ZBus(zbus::Error),
    /// polkit said no (or the user cancelled the password dialog).
    NotAuthorized(String),
    /// Another Kyu transaction is running.
    Busy(String),
    /// No such question pending (already answered, or never asked).
    NoQuestion(String),
    /// Only the user who started the update (or root) may answer or cancel.
    NotAllowed(String),
    /// Not one of pacman's answers to this question.
    InvalidAnswer(String),
    /// pacman's database lock exists: another package manager is running,
    /// or a previous run was interrupted.
    Locked(String),
    Failed(String),
}

pub struct Helper {
    pub shared: Arc<Shared>,
}

#[interface(name = "io.github.solarizeddev.kyu.Helper1")]
impl Helper {
    /// Runs `pacman -Syu` as root. Returns the transaction id; progress
    /// arrives as Event signals and in the log named by GetState.
    async fn start_update(
        &self,
        #[zbus(header)] header: Header<'_>,
        #[zbus(connection)] conn: &Connection,
    ) -> Result<String, HelperError> {
        self.shared.touch();
        let uid = caller_uid(conn, &header).await?;

        if self.shared.is_running() {
            // Answer before asking for a password that would not be used.
            return Err(HelperError::Busy("an update is already running".into()));
        }

        if !self.shared.config.session {
            authorize(conn, &header).await?;
        }

        let lock = &self.shared.config.db_lock;
        if lock.exists() {
            return Err(HelperError::Locked(format!(
                "{} exists. Another package manager may be running. If none is, a previous update was interrupted.",
                lock.display()
            )));
        }

        let inhibitor = if self.shared.config.session {
            None
        } else {
            inhibit(conn).await
        };
        let id = self.shared.begin(uid).map_err(|e| match e {
            BeginError::Busy(m) => HelperError::Busy(m),
            BeginError::Failed(m) => HelperError::Failed(m),
        })?;
        let shared = self.shared.clone();
        tokio::spawn(async move {
            transaction::run(shared.clone(), uid, inhibitor).await;
            if shared.exit_when_done() {
                eprintln!("kyu-helper: update finished, exiting as requested");
                std::process::exit(0);
            }
        });
        Ok(id)
    }

    /// Answers a relayed question (see the "question" record) with one of its
    /// answer values. Only the user who started the update, or root.
    async fn answer(
        &self,
        transaction: &str,
        question: u64,
        answer: &str,
        #[zbus(header)] header: Header<'_>,
        #[zbus(connection)] conn: &Connection,
    ) -> Result<(), HelperError> {
        self.shared.touch();
        let uid = caller_uid(conn, &header).await?;
        self.shared
            .answer(transaction, question, uid, answer)
            .map_err(reply_error)
    }

    /// Stops pacman while it waits for an answer, before it changes any
    /// package. Refused at any other time. Only the user who started the
    /// update, or root.
    async fn cancel(
        &self,
        transaction: &str,
        #[zbus(header)] header: Header<'_>,
        #[zbus(connection)] conn: &Connection,
    ) -> Result<(), HelperError> {
        self.shared.touch();
        let uid = caller_uid(conn, &header).await?;
        self.shared.cancel(transaction, uid).map_err(reply_error)
    }

    /// JSON object describing the helper's state; see docs/helper-protocol.md.
    async fn get_state(&self) -> String {
        self.shared.touch();
        self.shared.state_json()
    }

    /// One transaction log record (JSON), also appended to the log file.
    #[zbus(signal)]
    pub async fn event(
        emitter: &SignalEmitter<'_>,
        transaction: &str,
        seq: u64,
        record: &str,
    ) -> zbus::Result<()>;
}

fn reply_error(e: ReplyError) -> HelperError {
    match e {
        ReplyError::NoQuestion(m) => HelperError::NoQuestion(m),
        ReplyError::NotAllowed(m) => HelperError::NotAllowed(m),
        ReplyError::InvalidAnswer(m) => HelperError::InvalidAnswer(m),
    }
}

async fn caller_uid(conn: &Connection, header: &Header<'_>) -> Result<u32, HelperError> {
    let sender = header
        .sender()
        .ok_or_else(|| HelperError::Failed("message has no sender".into()))?
        .to_owned();
    fdo::DBusProxy::new(conn)
        .await?
        .get_connection_unix_user(sender.into())
        .await
        .map_err(|e| HelperError::Failed(format!("cannot identify the caller: {e}")))
}

async fn authorize(conn: &Connection, header: &Header<'_>) -> Result<(), HelperError> {
    let authority = AuthorityProxy::new(conn)
        .await
        .map_err(|e| HelperError::Failed(format!("cannot reach polkit: {e}")))?;
    let subject = Subject::new_for_message_header(header)
        .map_err(|e| HelperError::Failed(format!("cannot identify the caller for polkit: {e}")))?;
    let result = authority
        .check_authorization(
            &subject,
            POLKIT_ACTION,
            &HashMap::new(),
            CheckAuthorizationFlags::ALLOW_USER_INTERACTION,
            "",
        )
        .await
        .map_err(|e| HelperError::Failed(format!("polkit check failed: {e}")))?;
    if result.is_authorized {
        Ok(())
    } else {
        Err(HelperError::NotAuthorized(format!(
            "polkit did not authorize {POLKIT_ACTION}"
        )))
    }
}

/// Takes a logind "block" inhibitor for shutdown and sleep. Failure is not
/// fatal (logind may be missing); it is reported on stderr (the journal).
async fn inhibit(conn: &Connection) -> Option<OwnedFd> {
    let reply = conn
        .call_method(
            Some("org.freedesktop.login1"),
            "/org/freedesktop/login1",
            Some("org.freedesktop.login1.Manager"),
            "Inhibit",
            &(
                "shutdown:sleep",
                "Kyu",
                "Updating packages with pacman",
                "block",
            ),
        )
        .await;
    match reply.and_then(|m| m.body().deserialize::<zbus::OwnedFd>()) {
        Ok(fd) => Some(fd.into()),
        Err(e) => {
            eprintln!("kyu-helper: no shutdown inhibitor: {e}");
            None
        }
    }
}
