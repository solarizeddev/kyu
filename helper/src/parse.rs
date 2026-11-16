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
    /// Any other question: providers, conflicts, replacements, key imports,
    /// corrupted packages. Answering these is milestone 4; until then the
    /// helper stops pacman instead of picking an answer for the user.
    Unsupported,
}

/// Looks at the unterminated tail of the output (the text after the last
/// newline) and says whether pacman is asking something.
pub fn prompt(tail: &str) -> Option<Prompt> {
    let t = tail.trim_end_matches(' ');
    let is_question = t.ends_with("[Y/n]")
        || t.ends_with("[y/N]")
        || (t.starts_with("Enter a ") && t.ends_with("):"));
    if !is_question {
        return None;
    }
    if t.trim_start() == ":: Proceed with installation? [Y/n]" {
        Some(Prompt::Proceed)
    } else {
        Some(Prompt::Unsupported)
    }
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
    }

    #[test]
    fn prompts() {
        assert_eq!(
            prompt(":: Proceed with installation? [Y/n] "),
            Some(Prompt::Proceed)
        );
        assert_eq!(
            prompt(":: Replace foo with extra/bar? [Y/n] "),
            Some(Prompt::Unsupported)
        );
        assert_eq!(
            prompt(":: a and b are in conflict. Remove b? [y/N] "),
            Some(Prompt::Unsupported)
        );
        assert_eq!(
            prompt("Enter a number (default=1): "),
            Some(Prompt::Unsupported)
        );
        assert_eq!(
            prompt(":: Import PGP key 1234, \"Someone <a@b>\"? [Y/n] "),
            Some(Prompt::Unsupported)
        );
        assert_eq!(prompt("downloading kwin"), None);
        assert_eq!(prompt(""), None);
    }
}
