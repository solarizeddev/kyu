//! Classifies pacman's output. pacman runs with LC_ALL=C, --noprogressbar and
//! --color never, so its messages are stable English lines.
//!
//! Everything here is pure so it can be tested against recorded output.

/// What a complete output line tells us about the transaction.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LineInfo {
    /// "Packages (9) ..." or, with VerbosePkgLists, "Package (9) Old Version ...".
    PackageCount(u32),
    /// "upgrading kwin..." (printed per package with --noprogressbar).
    Operation {
        action: Action,
        package: String,
    },
    /// A line that starts a new stage of the run.
    Stage(Stage),
    /// " there is nothing to do".
    NothingToDo,
    /// "error: ..." (pacman's own wording, quoted later in the summary).
    Error(String),
    /// "warning: /etc/x installed as /etc/x.pacnew" (kind "pacnew") or
    /// "warning: /etc/x saved as /etc/x.pacsave" (kind "pacsave").
    Config {
        kind: &'static str,
        path: String,
        saved: String,
    },
    Other,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Action {
    Installing,
    Upgrading,
    Reinstalling,
    Downgrading,
    Removing,
}

impl Action {
    pub fn as_str(self) -> &'static str {
        match self {
            Action::Installing => "installing",
            Action::Upgrading => "upgrading",
            Action::Reinstalling => "reinstalling",
            Action::Downgrading => "downgrading",
            Action::Removing => "removing",
        }
    }

    /// Past tense for the summary: "9 upgraded".
    pub fn done_str(self) -> &'static str {
        match self {
            Action::Installing => "installed",
            Action::Upgrading => "upgraded",
            Action::Reinstalling => "reinstalled",
            Action::Downgrading => "downgraded",
            Action::Removing => "removed",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Stage {
    Sync,
    Resolve,
    Download,
    Check,
    Commit,
    Hooks,
}

impl Stage {
    pub fn as_str(self) -> &'static str {
        match self {
            Stage::Sync => "sync",
            Stage::Resolve => "resolve",
            Stage::Download => "download",
            Stage::Check => "check",
            Stage::Commit => "commit",
            Stage::Hooks => "hooks",
        }
    }
}

pub fn classify(line: &str) -> LineInfo {
    let trimmed = line.trim();
    if trimmed == "there is nothing to do" {
        return LineInfo::NothingToDo;
    }
    if let Some(rest) = trimmed.strip_prefix("warning: ") {
        for (marker, kind) in [(" installed as ", "pacnew"), (" saved as ", "pacsave")] {
            if let Some((path, saved)) = rest.split_once(marker)
                && saved.ends_with(&format!(".{kind}"))
            {
                return LineInfo::Config {
                    kind,
                    path: path.to_string(),
                    saved: saved.to_string(),
                };
            }
        }
    }
    if let Some(message) = trimmed.strip_prefix("error: ") {
        return LineInfo::Error(message.to_string());
    }
    if let Some(n) = package_count(trimmed) {
        return LineInfo::PackageCount(n);
    }
    if let Some(info) = operation(trimmed) {
        return info;
    }
    match trimmed {
        ":: Synchronizing package databases..." => LineInfo::Stage(Stage::Sync),
        ":: Starting full system upgrade..." => LineInfo::Stage(Stage::Resolve),
        ":: Retrieving packages..." => LineInfo::Stage(Stage::Download),
        "checking keyring..."
        | "checking package integrity..."
        | "loading package files..."
        | "checking for file conflicts..."
        | "checking available disk space..." => LineInfo::Stage(Stage::Check),
        ":: Processing package changes..." => LineInfo::Stage(Stage::Commit),
        ":: Running pre-transaction hooks..." | ":: Running post-transaction hooks..." => {
            LineInfo::Stage(Stage::Hooks)
        }
        _ => LineInfo::Other,
    }
}

fn package_count(line: &str) -> Option<u32> {
    let rest = line
        .strip_prefix("Packages (")
        .or_else(|| line.strip_prefix("Package ("))?;
    let end = rest.find(')')?;
    rest[..end].parse().ok()
}

fn operation(line: &str) -> Option<LineInfo> {
    let (verb, rest) = line.split_once(' ')?;
    let action = match verb {
        "installing" => Action::Installing,
        "upgrading" => Action::Upgrading,
        "reinstalling" => Action::Reinstalling,
        "downgrading" => Action::Downgrading,
        "removing" => Action::Removing,
        _ => return None,
    };
    let package = rest.strip_suffix("...")?;
    if package.is_empty() || package.contains(char::is_whitespace) {
        return None;
    }
    Some(LineInfo::Operation {
        action,
        package: package.to_string(),
    })
}

/// A question pacman is waiting on. pacman prints questions without a
/// trailing newline and flushes, so they show up as an unterminated tail.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Prompt {
    /// ":: Proceed with installation? [Y/n] ". The user already chose "Update all".
    Proceed,
    /// A question the panel can answer; see [`Question`].
    Relay(Question),
    /// Anything Kyu does not recognise. The helper stops pacman instead of
    /// guessing (fail closed).
    Unsupported,
}

/// A recognised question, with what it is about, parsed from pacman's text.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Question {
    /// provider, conflict, replace, import_key, corrupted, skip, ignored
    pub kind: &'static str,
    /// The question line as pacman printed it.
    pub prompt: String,
    /// The lines pacman printed for this question before the prompt.
    pub context: Vec<String>,
    /// Exactly what may be written back to pacman ("y"/"n" or provider numbers).
    pub answers: Vec<Answer>,
    /// pacman's own default.
    pub default: String,
    /// Kind-specific facts (package names, key, file), all from pacman's text.
    pub details: Vec<(&'static str, String)>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Answer {
    pub value: String,
    /// For providers: the package name; for yes/no: "yes" or "no".
    pub label: String,
    /// For providers: the repo.
    pub detail: String,
}

fn yes_no() -> Vec<Answer> {
    vec![
        Answer {
            value: "y".into(),
            label: "yes".into(),
            detail: String::new(),
        },
        Answer {
            value: "n".into(),
            label: "no".into(),
            detail: String::new(),
        },
    ]
}

/// Looks at the unterminated tail of the output (the text after the last
/// newline) and the recent complete lines, and says whether pacman is
/// asking something.
pub fn prompt(tail: &str, recent: &[String]) -> Option<Prompt> {
    let t = tail.trim_end_matches(' ');
    let yes_no_default = if t.ends_with("[Y/n]") {
        Some("y")
    } else if t.ends_with("[y/N]") {
        Some("n")
    } else {
        None
    };
    let numbered = t.starts_with("Enter a ") && t.ends_with("):");
    if yes_no_default.is_none() && !numbered {
        return None;
    }
    let text = t.trim_start();
    if text == ":: Proceed with installation? [Y/n]" {
        return Some(Prompt::Proceed);
    }
    let question = match yes_no_default {
        Some(default) => yes_no_question(text, default, recent),
        None => provider_question(text, recent),
    };
    Some(question.map(Prompt::Relay).unwrap_or(Prompt::Unsupported))
}

fn strip_marker(text: &str) -> &str {
    text.trim_end_matches("[Y/n]")
        .trim_end_matches("[y/N]")
        .trim_end()
}

fn yes_no_question(text: &str, default: &str, recent: &[String]) -> Option<Question> {
    let body = strip_marker(text);
    let q = |kind, context: Vec<String>, details| Question {
        kind,
        prompt: text.to_string(),
        context,
        answers: yes_no(),
        default: default.to_string(),
        details,
    };

    // ":: Replace %s with %s/%s?"
    if let Some(rest) = body
        .strip_prefix(":: Replace ")
        .and_then(|r| r.strip_suffix('?'))
    {
        let (old, new) = rest.split_once(" with ")?;
        if old.contains(' ') || new.contains(' ') {
            return None;
        }
        return Some(q(
            "replace",
            vec![],
            vec![("old", old.into()), ("new", new.into())],
        ));
    }
    // ":: %s and %s are in conflict. Remove %s?"
    // ":: %s and %s are in conflict (%s). Remove %s?"
    // Newer pacman versions may print name-version for the two packages.
    if let Some(rest) = body.strip_prefix(":: ")
        && let Some((pair, remove)) = rest.split_once(" are in conflict")
    {
        let (a, b) = pair.split_once(" and ")?;
        let remove = remove.rsplit_once(" Remove ")?.1.strip_suffix('?')?;
        let names = |p: &str, name: &str| {
            p == name || p.strip_prefix(name).is_some_and(|v| v.starts_with('-'))
        };
        let keep = if names(a, remove) {
            b
        } else if names(b, remove) {
            a
        } else {
            return None;
        };
        return Some(q(
            "conflict",
            vec![],
            vec![("install", keep.into()), ("remove", remove.into())],
        ));
    }
    // ":: Import PGP key FPR, \"UID\"?", older: "..., \"UID\", created: DATE?",
    // no uid: ":: Import PGP key FPR?"
    if let Some(rest) = body
        .strip_prefix(":: Import PGP key ")
        .and_then(|r| r.strip_suffix('?'))
    {
        let (key, uid) = match rest.split_once(", \"") {
            Some((key, rest)) => (key, rest.split('"').next().unwrap_or_default()),
            None => (rest, ""),
        };
        if key.is_empty() || key.contains(' ') {
            return None;
        }
        return Some(q(
            "import_key",
            vec![],
            vec![("key", key.into()), ("uid", uid.into())],
        ));
    }
    // ":: File %s is corrupted (%s).\nDo you want to delete it?"
    if body == "Do you want to delete it?" {
        // The ":: File ... is corrupted" line pacman printed just before.
        let line = recent
            .iter()
            .rev()
            .take(5)
            .find(|l| l.trim_start().starts_with(":: File "))?;
        let rest = line.trim_start().strip_prefix(":: File ")?;
        let (file, reason) = rest.split_once(" is corrupted (")?;
        let reason = reason.strip_suffix(").")?;
        return Some(q(
            "corrupted",
            vec![line.clone()],
            vec![("file", file.into()), ("reason", reason.into())],
        ));
    }
    // ":: The following package(s) cannot be upgraded due to unresolvable
    // dependencies:\n  pkgs\n\nDo you want to skip the above package(s) for this upgrade?"
    let unprefixed = body.strip_prefix(":: ").unwrap_or(body);
    if unprefixed == "Do you want to skip the above package for this upgrade?"
        || unprefixed == "Do you want to skip the above packages for this upgrade?"
    {
        let start = recent
            .iter()
            .rposition(|l| l.contains("cannot be upgraded due to unresolvable dependencies"))?;
        let context: Vec<String> = recent[start..].to_vec();
        let packages: Vec<String> = context[1..]
            .iter()
            .flat_map(|l| l.split_whitespace().map(str::to_string).collect::<Vec<_>>())
            .collect();
        // Skipping them would be a partial upgrade (HANDOFF.md rule 4): the
        // helper accepts no answer to this question, only Cancel.
        let mut question = q("skip", context, vec![("packages", packages.join(" "))]);
        question.answers.clear();
        return Some(question);
    }
    // "%s is in IgnorePkg/IgnoreGroup. Install anyway?"
    if let Some(package) = body.strip_suffix(" is in IgnorePkg/IgnoreGroup. Install anyway?") {
        let package = package.trim_start_matches(":: ");
        return Some(q("ignored", vec![], vec![("package", package.into())]));
    }
    None
}

/// ":: There are N providers available for DEP:\n:: Repository REPO\n   1) a  2) b\n\nEnter a number (default=1): "
fn provider_question(text: &str, recent: &[String]) -> Option<Question> {
    let default = text
        .strip_prefix("Enter a number (default=")?
        .strip_suffix("):")?;
    let start = recent
        .iter()
        .rposition(|l| l.starts_with(":: There are ") && l.contains(" providers available for "))?;
    let header = &recent[start];
    let dependency = header
        .split(" providers available for ")
        .nth(1)?
        .trim_end_matches(':');
    let mut answers = Vec::new();
    let mut repo = String::new();
    for line in &recent[start + 1..] {
        if let Some(r) = line.trim().strip_prefix(":: Repository ") {
            repo = r.to_string();
            continue;
        }
        // "   1) nvidia-utils  2) vulkan-intel  3) vulkan-radeon"
        let mut words = line.split_whitespace().peekable();
        while let Some(word) = words.next() {
            let Some(number) = word.strip_suffix(')') else {
                continue;
            };
            if number.parse::<u32>().is_err() {
                continue;
            }
            if let Some(name) = words.next() {
                answers.push(Answer {
                    value: number.to_string(),
                    label: name.to_string(),
                    detail: repo.clone(),
                });
            }
        }
    }
    if answers.is_empty() || !answers.iter().any(|a| a.value == default) {
        return None;
    }
    Some(Question {
        kind: "provider",
        prompt: text.to_string(),
        context: recent[start..].to_vec(),
        answers,
        default: default.to_string(),
        details: vec![("dependency", dependency.to_string())],
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn package_counts() {
        assert_eq!(
            classify("Packages (9) kwin-6.7.4-1  mesa-1:26.2.3-1"),
            LineInfo::PackageCount(9)
        );
        assert_eq!(
            classify("Package (12)        Old Version  New Version  Net Change"),
            LineInfo::PackageCount(12)
        );
        assert_eq!(classify("Packages (x) foo"), LineInfo::Other);
    }

    #[test]
    fn operations() {
        assert_eq!(
            classify("upgrading plasma-desktop..."),
            LineInfo::Operation {
                action: Action::Upgrading,
                package: "plasma-desktop".into()
            }
        );
        assert_eq!(
            classify("removing old-thing..."),
            LineInfo::Operation {
                action: Action::Removing,
                package: "old-thing".into()
            }
        );
        // Hook output that happens to start with a verb is not an operation.
        assert_eq!(
            classify("installing the new kernel image now"),
            LineInfo::Other
        );
        assert_eq!(classify("upgrading ..."), LineInfo::Other);
    }

    #[test]
    fn stages_and_errors() {
        assert_eq!(
            classify(":: Synchronizing package databases..."),
            LineInfo::Stage(Stage::Sync)
        );
        assert_eq!(
            classify(":: Processing package changes..."),
            LineInfo::Stage(Stage::Commit)
        );
        assert_eq!(
            classify(":: Running post-transaction hooks..."),
            LineInfo::Stage(Stage::Hooks)
        );
        assert_eq!(classify(" there is nothing to do"), LineInfo::NothingToDo);
        assert_eq!(
            classify("error: failed to commit transaction (conflicting files)"),
            LineInfo::Error("failed to commit transaction (conflicting files)".into())
        );
        assert_eq!(
            classify("(1/5) Arming ConditionNeedsUpdate..."),
            LineInfo::Other
        );
        assert_eq!(
            classify("warning: /etc/pacman.conf installed as /etc/pacman.conf.pacnew"),
            LineInfo::Config {
                kind: "pacnew",
                path: "/etc/pacman.conf".into(),
                saved: "/etc/pacman.conf.pacnew".into()
            }
        );
        assert_eq!(
            classify("warning: /etc/foo.conf saved as /etc/foo.conf.pacsave"),
            LineInfo::Config {
                kind: "pacsave",
                path: "/etc/foo.conf".into(),
                saved: "/etc/foo.conf.pacsave".into()
            }
        );
        assert_eq!(
            classify("warning: kwin: local (6.7.4-1) is newer than extra (6.7.3-1)"),
            LineInfo::Other
        );
    }

    #[test]
    fn prompts() {
        let none: Vec<String> = vec![];
        assert_eq!(
            prompt(":: Proceed with installation? [Y/n] ", &none),
            Some(Prompt::Proceed)
        );
        assert_eq!(prompt("downloading kwin", &none), None);
        assert_eq!(prompt("", &none), None);
        // Fail closed on anything unrecognised.
        assert_eq!(
            prompt(":: Something new? [Y/n] ", &none),
            Some(Prompt::Unsupported)
        );
        assert_eq!(
            prompt("Enter a selection (default=all): ", &none),
            Some(Prompt::Unsupported)
        );
    }

    fn relayed(tail: &str, recent: &[&str]) -> Question {
        let recent: Vec<String> = recent.iter().map(|s| s.to_string()).collect();
        match prompt(tail, &recent) {
            Some(Prompt::Relay(q)) => q,
            other => panic!("expected a relayed question, got {other:?}"),
        }
    }

    fn detail<'a>(q: &'a Question, key: &str) -> &'a str {
        &q.details.iter().find(|(k, _)| *k == key).unwrap().1
    }

    #[test]
    fn replace() {
        let q = relayed(":: Replace foo with extra/bar? [Y/n] ", &[]);
        assert_eq!(q.kind, "replace");
        assert_eq!(q.default, "y");
        assert_eq!(detail(&q, "old"), "foo");
        assert_eq!(detail(&q, "new"), "extra/bar");
    }

    #[test]
    fn conflict() {
        let q = relayed(":: foo and bar are in conflict. Remove bar? [y/N] ", &[]);
        assert_eq!((q.kind, q.default.as_str()), ("conflict", "n"));
        assert_eq!(detail(&q, "remove"), "bar");
        assert_eq!(detail(&q, "install"), "foo");
        let q = relayed(
            ":: foo and bar are in conflict (libx). Remove foo? [y/N] ",
            &[],
        );
        assert_eq!(detail(&q, "remove"), "foo");
        assert_eq!(detail(&q, "install"), "bar");
    }

    #[test]
    fn import_key() {
        let q = relayed(
            ":: Import PGP key 0123ABCD, \"Someone <s@example.org>\"? [Y/n] ",
            &[],
        );
        assert_eq!(q.kind, "import_key");
        assert_eq!(detail(&q, "key"), "0123ABCD");
        assert_eq!(detail(&q, "uid"), "Someone <s@example.org>");
    }

    #[test]
    fn corrupted() {
        let q = relayed(
            "Do you want to delete it? [Y/n] ",
            &[
                ":: File /var/cache/pacman/pkg/a-1-1.pkg.tar.zst is corrupted (invalid or corrupted package (PGP signature)).",
            ],
        );
        assert_eq!(q.kind, "corrupted");
        assert_eq!(
            detail(&q, "file"),
            "/var/cache/pacman/pkg/a-1-1.pkg.tar.zst"
        );
        assert_eq!(
            detail(&q, "reason"),
            "invalid or corrupted package (PGP signature)"
        );
        // Without the line before it, it is not recognised.
        assert_eq!(
            prompt("Do you want to delete it? [Y/n] ", &[]),
            Some(Prompt::Unsupported)
        );
    }

    #[test]
    fn skip() {
        let q = relayed(
            "Do you want to skip the above packages for this upgrade? [y/N] ",
            &[
                "checking dependencies...",
                ":: The following packages cannot be upgraded due to unresolvable dependencies:",
                "      foo  bar",
                "",
            ],
        );
        assert_eq!((q.kind, q.default.as_str()), ("skip", "n"));
        assert!(q.answers.is_empty(), "no answer may skip packages");
        assert_eq!(detail(&q, "packages"), "foo bar");
        assert_eq!(q.context.len(), 3);
    }

    #[test]
    fn real_pacman_prefixes_and_variants() {
        // pacman's question() puts ":: " before every yes/no question.
        let q = relayed(
            ":: Do you want to skip the above package for this upgrade? [y/N] ",
            &[
                ":: The following package cannot be upgraded due to unresolvable dependencies:",
                "      foo",
                "",
            ],
        );
        assert_eq!(q.kind, "skip");
        assert!(q.answers.is_empty());
        // Conflict with versions.
        let q = relayed(
            ":: foo-2.0-1 and bar-1.0-1 are in conflict. Remove bar? [y/N] ",
            &[],
        );
        assert_eq!(detail(&q, "remove"), "bar");
        assert_eq!(detail(&q, "install"), "foo-2.0-1");
        assert_eq!(
            prompt(
                ":: foo-2 and bar-1 are in conflict. Remove baz? [y/N] ",
                &[]
            ),
            Some(Prompt::Unsupported)
        );
        // Key without uid, and the older "created:" form.
        assert_eq!(
            detail(&relayed(":: Import PGP key 0123ABCD? [Y/n] ", &[]), "key"),
            "0123ABCD"
        );
        let q = relayed(
            ":: Import PGP key 4096R/0123ABCD, \"Someone <s@x>\", created: 2020-01-01? [Y/n] ",
            &[],
        );
        assert_eq!(detail(&q, "uid"), "Someone <s@x>");
        // Another line between the corrupted file and the question.
        let q = relayed(
            "Do you want to delete it? [Y/n] ",
            &[
                ":: File /var/cache/pacman/pkg/a.pkg.tar.zst is corrupted (invalid or corrupted package (checksum)).",
                "checking package integrity...",
            ],
        );
        assert_eq!(
            detail(&q, "reason"),
            "invalid or corrupted package (checksum)"
        );
    }

    #[test]
    fn provider() {
        let q = relayed(
            "Enter a number (default=1): ",
            &[
                "resolving dependencies...",
                ":: There are 4 providers available for vulkan-driver:",
                ":: Repository extra",
                "   1) nvidia-utils  2) vulkan-intel  3) vulkan-radeon",
                ":: Repository multilib",
                "   4) vulkan-swrast",
                "",
            ],
        );
        assert_eq!(q.kind, "provider");
        assert_eq!(q.default, "1");
        assert_eq!(detail(&q, "dependency"), "vulkan-driver");
        let names: Vec<_> = q
            .answers
            .iter()
            .map(|a| (a.value.as_str(), a.label.as_str(), a.detail.as_str()))
            .collect();
        assert_eq!(
            names,
            [
                ("1", "nvidia-utils", "extra"),
                ("2", "vulkan-intel", "extra"),
                ("3", "vulkan-radeon", "extra"),
                ("4", "vulkan-swrast", "multilib")
            ]
        );
        assert_eq!(
            q.context[0],
            ":: There are 4 providers available for vulkan-driver:"
        );
        // A default that is not among the options: fail closed.
        let recent = vec![
            ":: There are 1 providers available for x:".to_string(),
            "   1) a".to_string(),
        ];
        assert_eq!(
            prompt("Enter a number (default=7): ", &recent),
            Some(Prompt::Unsupported)
        );
    }

    #[test]
    fn ignored() {
        let q = relayed(
            ":: foo is in IgnorePkg/IgnoreGroup. Install anyway? [Y/n] ",
            &[],
        );
        assert_eq!(q.kind, "ignored");
        assert_eq!(detail(&q, "package"), "foo");
    }
}
