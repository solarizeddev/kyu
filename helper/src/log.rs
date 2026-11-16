//! Transaction log: one JSON object per line, the same records the helper
//! sends as D-Bus signals. A panel that starts (or restarts) mid-update reads
//! the file up to the current sequence number, then follows the signals.
//!
//! Format (version 1), common fields on every record:
//!   v     format version
//!   tx    transaction id
//!   seq   1, 2, 3, ... within the transaction
//!   time  UTC, RFC 3339
//!   kind  started | output | stage | packages | progress | answered | question | finished

use std::fs::{self, File, OpenOptions};
use std::io::{self, BufRead, BufReader, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use serde_json::{Map, Value};

pub const FORMAT_VERSION: u64 = 1;
/// Older logs are deleted when a new transaction starts.
const KEEP_LOGS: usize = 30;

pub struct TransactionLog {
    file: File,
    path: PathBuf,
}

impl TransactionLog {
    /// Creates `<dir>/<tx>.jsonl`, readable by everyone (like pacman.log):
    /// pacman's output is not secret, and the panel runs as the user.
    pub fn create(dir: &Path, tx: &str) -> io::Result<Self> {
        fs::DirBuilder::new()
            .recursive(true)
            .mode(0o755)
            .create(dir)?;
        prune(dir, KEEP_LOGS.saturating_sub(1))?;
        let path = dir.join(format!("{tx}.jsonl"));
        let file = OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o644)
            .open(&path)?;
        Ok(Self { file, path })
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn append(&mut self, record: &str) -> io::Result<()> {
        self.file.write_all(record.as_bytes())?;
        self.file.write_all(b"\n")?;
        // Flush per record: the log is what a reattaching panel reads.
        self.file.flush()
    }
}

fn log_files(dir: &Path) -> io::Result<Vec<PathBuf>> {
    let mut files: Vec<PathBuf> = match fs::read_dir(dir) {
        Ok(entries) => entries
            .filter_map(|e| e.ok().map(|e| e.path()))
            .filter(|p| p.extension().is_some_and(|ext| ext == "jsonl"))
            .collect(),
        Err(e) if e.kind() == io::ErrorKind::NotFound => Vec::new(),
        Err(e) => return Err(e),
    };
    // Transaction ids start with a UTC timestamp, so names sort by time.
    files.sort();
    Ok(files)
}

fn prune(dir: &Path, keep: usize) -> io::Result<()> {
    let files = log_files(dir)?;
    if files.len() > keep {
        for old in &files[..files.len() - keep] {
            let _ = fs::remove_file(old);
        }
    }
    Ok(())
}

/// The newest log and its last record, to report the previous transaction
/// after the helper restarts (it exits when idle).
pub fn newest(dir: &Path) -> Option<(PathBuf, Map<String, Value>)> {
    let path = log_files(dir).ok()?.pop()?;
    let reader = BufReader::new(File::open(&path).ok()?);
    let last = reader
        .lines()
        .map_while(Result::ok)
        .filter(|l| !l.trim().is_empty())
        .last()?;
    let record = serde_json::from_str::<Value>(&last)
        .ok()?
        .as_object()?
        .clone();
    Some((path, record))
}

/// Current time as RFC 3339 UTC, e.g. 2026-11-16T09:52:15Z.
pub fn now_rfc3339() -> String {
    let (y, mo, d, h, mi, s) = civil_now();
    format!("{y:04}-{mo:02}-{d:02}T{h:02}:{mi:02}:{s:02}Z")
}

/// A transaction id: compact UTC timestamp plus a two-digit counter that
/// keeps ids unique within one second, e.g. 20261116T095215Z-00. Ids (and
/// so log file names) sort by time.
pub fn transaction_id(attempt: u32) -> String {
    let (y, mo, d, h, mi, s) = civil_now();
    format!("{y:04}{mo:02}{d:02}T{h:02}{mi:02}{s:02}Z-{attempt:02}")
}

fn civil_now() -> (i64, u32, u32, u32, u32, u32) {
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs() as i64)
        .unwrap_or(0);
    let (days, rem) = (secs.div_euclid(86_400), secs.rem_euclid(86_400));
    let (y, m, d) = civil_from_days(days);
    (
        y,
        m,
        d,
        (rem / 3600) as u32,
        (rem % 3600 / 60) as u32,
        (rem % 60) as u32,
    )
}

/// Howard Hinnant's days-to-civil algorithm.
fn civil_from_days(z: i64) -> (i64, u32, u32) {
    let z = z + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z.rem_euclid(146_097);
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let m = if mp < 10 { mp + 3 } else { mp - 9 } as u32;
    (if m <= 2 { y + 1 } else { y }, m, d)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn civil_dates() {
        assert_eq!(civil_from_days(0), (1970, 1, 1));
        assert_eq!(civil_from_days(20_408), (2025, 11, 16));
        assert_eq!(civil_from_days(11_016), (2000, 2, 29));
    }

    #[test]
    fn create_prune_and_read_newest() {
        let dir = std::env::temp_dir().join(format!("kyu-log-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        for i in 0..KEEP_LOGS + 3 {
            let mut log = TransactionLog::create(&dir, &format!("2026{i:04}")).unwrap();
            log.append(&format!(r#"{{"seq":1,"kind":"started","n":{i}}}"#))
                .unwrap();
            log.append(&format!(r#"{{"seq":2,"kind":"finished","n":{i}}}"#))
                .unwrap();
        }
        assert_eq!(log_files(&dir).unwrap().len(), KEEP_LOGS);
        let (path, last) = newest(&dir).unwrap();
        assert!(path.ends_with(format!("2026{:04}.jsonl", KEEP_LOGS + 2)));
        assert_eq!(last["kind"], "finished");
        fs::remove_dir_all(&dir).unwrap();
    }
}
