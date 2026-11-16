//! What an update leaves to do afterwards, derived from the system rather
//! than from lists of packages (HANDOFF.md rule 7):
//!
//! - the kernel: the running kernel's modules directory is gone, so the
//!   running kernel has been replaced and modules for new hardware cannot be
//!   loaded until a restart;
//! - processes that still map shared libraries the update replaced (the
//!   mapping shows the old file as "(deleted)").
//!
//! A restart is "required" when the kernel was replaced or PID 1 (systemd)
//! uses a replaced library. Other processes are listed by name: restarting
//! them, or logging out, is enough for those.

use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};

use serde_json::{Value, json};

/// Where to look; /proc and /usr/lib/modules except in tests.
pub struct Roots {
    pub proc_dir: PathBuf,
    pub modules_dir: PathBuf,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Kernel {
    pub running: String,
    /// The installed version of the running kernel's package, when it can be
    /// told (from BOOT_IMAGE=/boot/vmlinuz-<pkgbase> and each module
    /// directory's pkgbase file).
    pub installed: Option<String>,
    pub replaced: bool,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Process {
    pub pid: u32,
    pub name: String,
    pub libraries: Vec<String>,
}

fn read_trimmed(path: &Path) -> Option<String> {
    fs::read_to_string(path)
        .ok()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
}

pub fn kernel(roots: &Roots) -> Option<Kernel> {
    let running = read_trimmed(&roots.proc_dir.join("sys/kernel/osrelease"))?;
    // No modules directory at all (a container): nothing to compare with.
    let dirs: Vec<PathBuf> = fs::read_dir(&roots.modules_dir)
        .ok()?
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.join("vmlinuz").is_file())
        .collect();
    if dirs.is_empty() {
        return None;
    }
    if roots.modules_dir.join(&running).is_dir() {
        return Some(Kernel {
            installed: Some(running.clone()),
            running,
            replaced: false,
        });
    }
    let pkgbase = read_trimmed(&roots.proc_dir.join("cmdline")).and_then(|cmdline| {
        cmdline
            .split_whitespace()
            .find_map(|arg| arg.strip_prefix("BOOT_IMAGE="))
            .and_then(|image| image.rsplit('/').next())
            .and_then(|file| file.strip_prefix("vmlinuz-"))
            .map(str::to_string)
    });
    let name = |p: &PathBuf| p.file_name().map(|n| n.to_string_lossy().into_owned());
    let installed = match pkgbase {
        Some(pkgbase) => dirs
            .iter()
            .find(|d| read_trimmed(&d.join("pkgbase")).as_deref() == Some(pkgbase.as_str()))
            .and_then(name),
        None if dirs.len() == 1 => name(&dirs[0]),
        None => None,
    };
    Some(Kernel {
        running,
        installed,
        replaced: true,
    })
}

/// A path from /proc/PID/maps that is a replaced shared library.
fn replaced_library(line: &str) -> Option<String> {
    // address perms offset dev inode pathname; the pathname may contain spaces.
    let mut rest = line;
    for _ in 0..5 {
        rest = rest.trim_start().split_once(char::is_whitespace)?.1;
    }
    let path = rest.trim_start().strip_suffix(" (deleted)")?;
    let in_lib = ["/usr/lib/", "/usr/lib64/", "/lib/", "/lib64/"]
        .iter()
        .any(|p| path.starts_with(p));
    let file = path.rsplit('/').next()?;
    let shared = file.ends_with(".so") || file.contains(".so.");
    (in_lib && shared).then(|| path.to_string())
}

pub fn processes(roots: &Roots) -> Vec<Process> {
    let mut found = Vec::new();
    let Ok(entries) = fs::read_dir(&roots.proc_dir) else {
        return found;
    };
    for entry in entries.flatten() {
        let Some(pid) = entry
            .file_name()
            .to_str()
            .and_then(|n| n.parse::<u32>().ok())
        else {
            continue;
        };
        // Processes may exit, and some maps are not readable: skip those.
        let Ok(maps) = fs::read_to_string(entry.path().join("maps")) else {
            continue;
        };
        let libraries: BTreeSet<String> = maps.lines().filter_map(replaced_library).collect();
        if libraries.is_empty() {
            continue;
        }
        let name = read_trimmed(&entry.path().join("comm")).unwrap_or_else(|| pid.to_string());
        found.push(Process {
            pid,
            name,
            libraries: libraries.into_iter().collect(),
        });
    }
    found.sort_by(|a, b| a.name.cmp(&b.name).then(a.pid.cmp(&b.pid)));
    found
}

/// The "restart" object of the finished record.
pub fn restart(roots: &Roots) -> Value {
    let kernel = kernel(roots);
    let processes = processes(roots);
    let kernel_replaced = kernel.as_ref().is_some_and(|k| k.replaced);
    let pid1 = processes.iter().any(|p| p.pid == 1);
    json!({
        "required": kernel_replaced || pid1,
        "kernel": kernel.map(|k| json!({ "running": k.running, "installed": k.installed, "replaced": k.replaced })),
        "systemd": pid1,
        // Listing every process can be long; the first 200 by name.
        "processes": processes.iter().take(200).map(|p| json!({ "pid": p.pid, "name": p.name, "libraries": p.libraries })).collect::<Vec<_>>(),
        "processCount": processes.len(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Tree(PathBuf);
    impl Tree {
        fn new(name: &str) -> Self {
            let dir =
                std::env::temp_dir().join(format!("kyu-aftercare-{name}-{}", std::process::id()));
            let _ = fs::remove_dir_all(&dir);
            Tree(dir)
        }
        fn file(&self, path: &str, content: &str) -> &Self {
            let p = self.0.join(path);
            fs::create_dir_all(p.parent().unwrap()).unwrap();
            fs::write(p, content).unwrap();
            self
        }
        fn roots(&self) -> Roots {
            Roots {
                proc_dir: self.0.join("proc"),
                modules_dir: self.0.join("modules"),
            }
        }
    }
    impl Drop for Tree {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    #[test]
    fn running_kernel_still_installed() {
        let t = Tree::new("same");
        t.file("proc/sys/kernel/osrelease", "6.18.22-1-MANJARO\n")
            .file("modules/6.18.22-1-MANJARO/vmlinuz", "")
            .file("modules/6.18.22-1-MANJARO/pkgbase", "linux618\n");
        let k = kernel(&t.roots()).unwrap();
        assert!(!k.replaced);
    }

    #[test]
    fn running_kernel_replaced() {
        let t = Tree::new("replaced");
        t.file("proc/sys/kernel/osrelease", "6.18.20-1-MANJARO\n")
            .file(
                "proc/cmdline",
                "BOOT_IMAGE=/boot/vmlinuz-linux618 root=UUID=x rw quiet\n",
            )
            .file("modules/6.18.22-1-MANJARO/vmlinuz", "")
            .file("modules/6.18.22-1-MANJARO/pkgbase", "linux618\n")
            .file("modules/6.12.60-1-MANJARO/vmlinuz", "")
            .file("modules/6.12.60-1-MANJARO/pkgbase", "linux612\n");
        assert_eq!(
            kernel(&t.roots()),
            Some(Kernel {
                running: "6.18.20-1-MANJARO".into(),
                installed: Some("6.18.22-1-MANJARO".into()),
                replaced: true
            })
        );
    }

    #[test]
    fn no_modules_directory_means_unknown() {
        let t = Tree::new("container");
        t.file("proc/sys/kernel/osrelease", "6.18.20-1\n");
        assert_eq!(kernel(&t.roots()), None);
    }

    #[test]
    fn maps_lines() {
        assert_eq!(
            replaced_library(
                "7f0e1c000000-7f0e1c028000 r--p 00000000 00:1f 123  /usr/lib/libc.so.6 (deleted)"
            ),
            Some("/usr/lib/libc.so.6".into())
        );
        assert_eq!(
            replaced_library("7f0e-7f0f r--p 0 00:1f 123  /usr/lib/libc.so.6"),
            None
        );
        assert_eq!(
            replaced_library("7f0e-7f0f rw-s 0 00:01 9  /memfd:wayland (deleted)"),
            None
        );
        assert_eq!(
            replaced_library("7f0e-7f0f r--p 0 00:1f 9  /home/a/b.so (deleted)"),
            None
        );
        assert_eq!(
            replaced_library("7f0e-7f0f r--p 0 00:1f 9  /usr/lib/locale/locale-archive (deleted)"),
            None
        );
        assert_eq!(
            replaced_library("7f0e-7f0f r-xp 0 00:1f 9  /usr/lib/qt6/plugins/a b.so (deleted)"),
            Some("/usr/lib/qt6/plugins/a b.so".into())
        );
    }

    #[test]
    fn restart_required_by_systemd_or_kernel() {
        let t = Tree::new("processes");
        t.file("proc/1/comm", "systemd\n")
            .file("proc/1/maps", "1-2 r--p 0 00:1f 9  /usr/lib/systemd/libsystemd-shared-258.so (deleted)\n")
            .file("proc/42/comm", "plasmashell\n")
            .file("proc/42/maps", "1-2 r--p 0 00:1f 9  /usr/lib/libQt6Core.so.6.10.1 (deleted)\n1-2 r--p 0 00:1f 9  /usr/lib/libQt6Core.so.6.10.1 (deleted)\n")
            .file("proc/43/comm", "bash\n")
            .file("proc/43/maps", "1-2 r--p 0 00:1f 9  /usr/lib/libc.so.6\n");
        let r = restart(&t.roots());
        assert_eq!(r["required"], true);
        assert_eq!(r["systemd"], true);
        assert_eq!(r["kernel"], Value::Null);
        assert_eq!(r["processCount"], 2);
        assert_eq!(r["processes"][0]["name"], "plasmashell");
        assert_eq!(r["processes"][0]["libraries"].as_array().unwrap().len(), 1);

        let t = Tree::new("none");
        t.file("proc/43/comm", "bash\n")
            .file("proc/43/maps", "1-2 r--p 0 00:1f 9  /usr/lib/libc.so.6\n");
        assert_eq!(restart(&t.roots())["required"], false);
    }
}
