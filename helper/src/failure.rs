//! Why an update failed, read from pacman's own lines (LC_ALL=C), so the
//! panel can say what happened and what usually fixes it. The lines are kept
//! verbatim as evidence; the kind only picks the explanation.

use serde_json::{Value, json};

/// Lines worth keeping while pacman runs (bounded).
#[derive(Default)]
pub struct Evidence {
    lines: Vec<String>,
}

const KEEP: usize = 200;

fn notable(line: &str) -> bool {
    let l = line.trim();
    l.starts_with("error:")
        || l.starts_with("warning: failed")
        || l.ends_with(" exists in filesystem")
        || l.contains("breaks dependency")
        || l.contains("unable to satisfy dependency")
        || l.starts_with("Errors occurred")
}

impl Evidence {
    pub fn add(&mut self, line: &str) {
        if self.lines.len() < KEEP && notable(line) {
            self.lines.push(line.trim().to_string());
        }
    }

    pub fn classify(&self) -> Option<Value> {
        let has = |needle: &str| self.lines.iter().any(|l| l.contains(needle));
        let matching = |pred: &dyn Fn(&str) -> bool| -> Vec<String> {
            self.lines
                .iter()
                .filter(|l| pred(l))
                .take(20)
                .cloned()
                .collect()
        };
        let retrieving: Vec<&String> = self
            .lines
            .iter()
            .filter(|l| l.contains("failed retrieving file"))
            .collect();

        let (kind, evidence, extra) = if has("unable to lock database")
            || has("could not lock database")
        {
            ("lock", matching(&|l| l.contains("lock")), json!({}))
        } else if !retrieving.is_empty() && retrieving.iter().all(|l| network_reason(l)) {
            let hosts = unique(retrieving.iter().filter_map(|l| host(l)));
            (
                "network",
                retrieving.iter().take(20).map(|s| s.to_string()).collect(),
                json!({ "hosts": hosts }),
            )
        } else if retrieving.iter().any(|l| missing_on_mirror(l)) {
            let files = unique(
                retrieving
                    .iter()
                    .filter(|l| missing_on_mirror(l))
                    .filter_map(|l| quoted(l)),
            );
            let hosts = unique(
                retrieving
                    .iter()
                    .filter(|l| missing_on_mirror(l))
                    .filter_map(|l| host(l)),
            );
            (
                "mirror",
                retrieving.iter().take(20).map(|s| s.to_string()).collect(),
                json!({ "files": files, "hosts": hosts }),
            )
        } else if has("unknown trust")
            || has("marginal trust")
            || has("(PGP signature)")
            || has("required key missing from keyring")
            || has("could not be looked up remotely")
            || has("unknown public key")
            || (has("signature from") && has("is invalid"))
        {
            let packages = unique(
                self.lines
                    .iter()
                    .filter(|l| l.contains("signature from"))
                    .filter_map(|l| {
                        l.strip_prefix("error: ")?
                            .split(':')
                            .next()
                            .map(str::to_string)
                    }),
            );
            (
                "keyring",
                matching(&|l| l.contains("signature") || l.contains("key") || l.contains("PGP")),
                json!({ "packages": packages }),
            )
        } else if has("conflicting files") {
            let files: Vec<String> = self
                .lines
                .iter()
                .filter(|l| l.ends_with(" exists in filesystem"))
                .map(|l| l.trim_end_matches(" exists in filesystem").to_string())
                .collect();
            let count = files.len();
            (
                "files",
                matching(&|l| l.contains("conflicting files") || l.starts_with("Errors occurred")),
                json!({ "files": files.into_iter().take(50).collect::<Vec<_>>(), "fileCount": count }),
            )
        } else if has("too full") || has("not enough free disk space") {
            (
                "disk",
                matching(&|l| l.contains("too full") || l.contains("disk space")),
                json!({}),
            )
        } else if has("could not satisfy dependencies") {
            (
                "dependencies",
                matching(&|l| l.contains("dependenc") || l.starts_with("error:")),
                json!({}),
            )
        } else {
            return None;
        };
        let mut failure = json!({ "kind": kind, "lines": evidence });
        if let (Value::Object(f), Value::Object(e)) = (&mut failure, extra) {
            f.extend(e);
        }
        Some(failure)
    }
}

fn unique(items: impl Iterator<Item = String>) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    for i in items {
        if !out.contains(&i) {
            out.push(i);
        }
    }
    out
}

/// "error: failed retrieving file 'NAME' from HOST : REASON"
fn quoted(line: &str) -> Option<String> {
    let start = line.find('\'')? + 1;
    let end = start + line[start..].find('\'')?;
    Some(line[start..end].to_string())
}

fn host(line: &str) -> Option<String> {
    let rest = line.split("' from ").nth(1)?;
    Some(rest.split(" : ").next()?.trim().to_string())
}

fn reason(line: &str) -> &str {
    line.rsplit(" : ").next().unwrap_or("")
}

fn network_reason(line: &str) -> bool {
    let r = reason(line);
    [
        "Could not resolve host",
        "Couldn't resolve host",
        "Failed to connect",
        "Could not connect",
        "Connection timed out",
        "Connection refused",
        "Network is unreachable",
        "Operation too slow",
        "Resolving timed out",
        "Connection reset",
    ]
    .iter()
    .any(|n| r.contains(n))
}

fn missing_on_mirror(line: &str) -> bool {
    let r = reason(line);
    r.contains("returned error: 404") || r.contains("returned error: 410")
}

#[cfg(test)]
mod tests {
    use super::*;

    fn classify(lines: &[&str]) -> Option<Value> {
        let mut e = Evidence::default();
        for l in lines {
            e.add(l);
        }
        e.classify()
    }

    #[test]
    fn network() {
        let f = classify(&[
            ":: Synchronizing package databases...",
            "error: failed retrieving file 'core.db' from geo.mirror.pkgbuild.com : Could not resolve host: geo.mirror.pkgbuild.com",
            "error: failed retrieving file 'extra.db' from mirror.example.org : Failed to connect to mirror.example.org port 443 after 3 ms: Couldn't connect to server",
            "error: failed to synchronize all databases (failed to retrieve some files)",
        ])
        .unwrap();
        assert_eq!(f["kind"], "network");
        assert_eq!(
            f["hosts"],
            json!(["geo.mirror.pkgbuild.com", "mirror.example.org"])
        );
    }

    #[test]
    fn mirror_out_of_sync() {
        let f = classify(&[
            "error: failed retrieving file 'mesa-1:26.2.3-1-x86_64.pkg.tar.zst' from mirror.example.org : The requested URL returned error: 404",
            "warning: failed to retrieve some files",
            "error: failed to commit transaction (failed to retrieve some files)",
            "Errors occurred, no packages were upgraded.",
        ])
        .unwrap();
        assert_eq!(f["kind"], "mirror");
        assert_eq!(f["files"], json!(["mesa-1:26.2.3-1-x86_64.pkg.tar.zst"]));
        assert_eq!(f["hosts"], json!(["mirror.example.org"]));
    }

    #[test]
    fn keyring() {
        let f = classify(&[
            "error: mesa: signature from \"Someone <s@example.org>\" is unknown trust",
            "error: failed to commit transaction (invalid or corrupted package (PGP signature))",
        ])
        .unwrap();
        assert_eq!(f["kind"], "keyring");
        assert_eq!(f["packages"], json!(["mesa"]));
        assert_eq!(
            classify(&["error: required key missing from keyring"]).unwrap()["kind"],
            "keyring"
        );
    }

    #[test]
    fn conflicting_files() {
        let f = classify(&[
            "error: failed to commit transaction (conflicting files)",
            "foo: /usr/bin/foo exists in filesystem",
            "foo: /usr/share/foo/data exists in filesystem",
            "Errors occurred, no packages were upgraded.",
        ])
        .unwrap();
        assert_eq!(f["kind"], "files");
        assert_eq!(
            f["files"],
            json!(["foo: /usr/bin/foo", "foo: /usr/share/foo/data"])
        );
        assert_eq!(f["fileCount"], 2);
    }

    #[test]
    fn disk_lock_dependencies_and_unknown() {
        assert_eq!(
            classify(&[
                "error: Partition /var too full: 2000 blocks needed, 100 blocks free",
                "error: failed to commit transaction (not enough free disk space)"
            ])
            .unwrap()["kind"],
            "disk"
        );
        assert_eq!(
            classify(&[
                "error: failed to init transaction (unable to lock database)",
                "error: could not lock database: File exists"
            ])
            .unwrap()["kind"],
            "lock"
        );
        assert_eq!(
            classify(&[
                "error: failed to prepare transaction (could not satisfy dependencies)",
                ":: installing foo (2-1) breaks dependency 'foo=1' required by bar"
            ])
            .unwrap()["kind"],
            "dependencies"
        );
        assert_eq!(classify(&["error: something new"]), None);
        // Not a failure line at all.
        assert_eq!(classify(&["upgrading kwin..."]), None);
    }
}
