#pragma once

#include "updatestate.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>

#include <optional>

namespace Kyu
{
// What the tray icon shows, most urgent first.
enum class TrayMark { Updating, Error, Restart, News, Updates, UpToDate };

struct TrayInput {
    std::optional<UpdateState> state;
    bool checking = false;
    QString loadError;
    QJsonObject lastRun; // kyu-update's last-run.json
    QStringList configs; // the configuration files (.pacnew, .pacsave) lastRun lists that still exist
    bool sameBoot = false; // lastRun was written in this boot
    bool kernelReplaced = false; // the running kernel's modules are gone
    QString kernelInstalled; // the installed version of the running kernel's package, when known
    bool updating = false; // a kyu-update holds its lock
    QString activity; // what that kyu-update does: "update" or "merge"
    QStringList newsSeen; // announcements already read (news.h)
    QStringList aurFailed; // AUR packages whose listed version was not updated last time (left out of state)
    QDateTime now;
};

// An AUR package whose listed version did not build (lastRun's "aurFailed")
// is not offered again until the AUR has a newer one: taken out of state,
// named in aurFailed.
void leaveOutFailedAur(TrayInput &in);

// Everything but the checker's state, read from the system now: kyu-update's
// last-run.json, lock and activity, the boot, the running kernel.
TrayInput currentTrayInput(const std::optional<UpdateState> &state, bool checking, const QString &loadError);

struct TrayView {
    TrayMark mark = TrayMark::UpToDate;
    QString iconName; // a theme icon (empty for Updating: Kyu's own frames)
    QString tooltip; // one or two lines
    bool passive = true; // nothing to do: the tray may hide it
    bool lastFailed = false; // Error because the last update failed (not the check)
    // An announcement not read yet (with updates pending or not): the
    // tray's "News" notification, once per announcement.
    QString newsTitle;
    QString newsLink;
    QString newsKey;
};

TrayView trayView(const TrayInput &in);

// One row of the card's Details list: what an update changes.
struct DetailRow {
    QString name;
    QString oldVersion; // empty for a Flatpak
    QString newVersion; // a Flatpak's branch
    QString source; // the repo, "AUR" or "Flatpak"
    qint64 size = -1; // the package's download size in bytes; -1 unknown (the AUR, Flatpaks)

    bool operator==(const DetailRow &) const = default;
};

// Details as pacman lists packages (its VerbosePkgLists table): monospace
// lines, columns padded with spaces; a block per source, as the card's
// line counts them: "Package (6)" (repo/name, Old Version, New Version,
// Download Size when known), "AUR (1)", "Flatpak (2)" (Branch). A header line
// is drawn bold.
struct DetailLine {
    QString text;
    bool header = false;

    bool operator==(const DetailLine &) const = default;
};
QList<DetailLine> detailLines(const QList<DetailRow> &rows);

// The card the tray opens (a small dialog): an icon, a heading, two lines,
// one more line (a link to read first, or a note), and at most two buttons;
// Details below on request.
struct CardView {
    enum class Kind { NotChecked, Checking, UpToDate, CheckFailed, Updates, Updating, Merging, Restart, Settings, Failed };
    enum class Action { None, Update, Restart, Merge, Check, ShowWindow };
    Kind kind = Kind::NotChecked;
    QString icon; // a theme icon (Breeze's monochrome ones)
    bool busy = false; // something runs: a progress bar
    QString heading;
    QString detail; // the body: one sentence or fact line
    bool detailPath = false; // the body ends in a path: one line, shortened in the middle, never wrapped
    QString linkText; // "Read first: …"
    QString linkUrl;
    QString linkKey; // the announcement's key (news.h), marked read when opened
    QString muted; // a quieter line under the detail: times, "Requires a restart", a file to merge
    QString failureKind; // Failed: the failure.h kind; the card puts its title before the detail
    bool transactionStarted = false; // Failed: the failing pacman run had begun changing packages
    int changed = 0; // Failed: packages an earlier pacman run of the update changed (the keyrings first)
    Action action = Action::None;
    QString actionText;
    bool primary = false; // the main thing to do (the default button) or just available
    bool actionEnabled = true; // false: shown greyed out ("Checking…")
    bool secondaryEnabled = true;
    Action secondary = Action::None; // a second button, left of the first
    QString secondaryText;
    QList<DetailRow> rows; // Details: what the update changes
    QString raw; // Details: the failure as the program said it

    bool operator==(const CardView &) const = default;
};

CardView cardView(const TrayInput &in);

// "/etc/sddm.conf" for "/etc/sddm.conf.pacnew" or ".pacsave".
QString configTarget(const QString &path);
}
