//! pacman's database lock. libalpm creates db.lck with O_EXCL and keeps it
//! open for the whole transaction, so the process holding it open is the one
//! using the database. A lock file nobody holds is left over from a run that
//! was killed; it can be removed.

use std::fs;
use std::path::Path;

use serde_json::{Value, json};

pub struct Holder {
    pub pid: u32,
    pub name: String,
}

fn comm(proc_dir: &Path, pid: &str) -> String {
    fs::read_to_string(proc_dir.join(pid).join("comm"))
        .map(|s| s.trim().to_string())
        .unwrap_or_default()
}

/// The process that has `lock` open, if any.
pub fn holder(proc_dir: &Path, lock: &Path) -> Option<Holder> {
    for entry in fs::read_dir(proc_dir).ok()?.flatten() {
        let pid_name = entry.file_name().to_string_lossy().into_owned();
        let Ok(pid) = pid_name.parse::<u32>() else {
            continue;
        };
        let Ok(fds) = fs::read_dir(entry.path().join("fd")) else {
            continue;
        };
        for fd in fds.flatten() {
            if fs::read_link(fd.path()).is_ok_and(|target| target == lock) {
                return Some(Holder {
                    pid,
                    name: comm(proc_dir, &pid_name),
                });
            }
        }
    }
    None
}

/// A running process named pacman, held lock or not (belt and braces).
pub fn pacman_running(proc_dir: &Path) -> Option<Holder> {
    for entry in fs::read_dir(proc_dir).ok()?.flatten() {
        let pid_name = entry.file_name().to_string_lossy().into_owned();
        let Ok(pid) = pid_name.parse::<u32>() else {
            continue;
        };
        let name = comm(proc_dir, &pid_name);
        if name == "pacman" {
            return Some(Holder { pid, name });
        }
    }
    None
}

/// The "lock" object of GetState.
pub fn state(proc_dir: &Path, lock: &Path) -> Value {
    let exists = lock.exists();
    let holder = if exists { holder(proc_dir, lock) } else { None };
    json!({
        "path": lock,
        "exists": exists,
        "holder": holder.map(|h| json!({ "pid": h.pid, "name": h.name })),
    })
}

/// Why `lock` must not be removed now, or None if it is stale.
pub fn in_use(proc_dir: &Path, lock: &Path) -> Option<String> {
    if let Some(h) = holder(proc_dir, lock) {
        return Some(format!(
            "{} is held by pid {} ({})",
            lock.display(),
            h.pid,
            h.name
        ));
    }
    if let Some(p) = pacman_running(proc_dir) {
        return Some(format!("pacman is running (pid {})", p.pid));
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::unix::fs::symlink;

    #[test]
    fn finds_the_holder() {
        let dir = std::env::temp_dir().join(format!("kyu-lock-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        let lock = dir.join("db.lck");
        fs::create_dir_all(dir.join("proc/77/fd")).unwrap();
        fs::create_dir_all(dir.join("proc/78/fd")).unwrap();
        fs::write(&lock, "").unwrap();
        fs::write(dir.join("proc/77/comm"), "pamac-daemon\n").unwrap();
        fs::write(dir.join("proc/78/comm"), "bash\n").unwrap();
        symlink("/dev/null", dir.join("proc/78/fd/0")).unwrap();
        assert!(in_use(&dir.join("proc"), &lock).is_none());
        symlink(&lock, dir.join("proc/77/fd/5")).unwrap();
        let reason = in_use(&dir.join("proc"), &lock).unwrap();
        assert!(reason.contains("pid 77 (pamac-daemon)"), "{reason}");
        assert_eq!(
            state(&dir.join("proc"), &lock)["holder"]["name"],
            "pamac-daemon"
        );
        fs::remove_dir_all(&dir).unwrap();
    }
}
