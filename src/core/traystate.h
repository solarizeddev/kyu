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
    QStringList configs; // the settings files lastRun lists that still exist
    bool sameBoot = false; // lastRun was written in this boot
    bool kernelReplaced = false; // the running kernel's modules are gone
    QString kernelInstalled; // the installed version of the running kernel's package, when known
    bool updating = false; // a kyu-update holds its lock
    QString activity; // what that kyu-update does: "update" or "merge"
    QDateTime now;
};

// Everything but the checker's state, read from the system now: kyu-update's
// last-run.json, lock and activity, the boot, the running kernel.
TrayInput currentTrayInput(const std::optional<UpdateState> &state, bool checking, const QString &loadError);

struct TrayView {
    TrayMark mark = TrayMark::UpToDate;
    QString iconName; // a theme icon (empty for Updating: Kyu's own frames)
    QString tooltip; // one or two lines
    bool passive = true; // nothing to do: the tray may hide it
    bool lastFailed = false; // Error because the last update failed (not the check)
};

TrayView trayView(const TrayInput &in);

// The card the tray opens: one heading, one line, at most one link and one
// extra row, one button.
struct CardView {
    enum class Kind { NotChecked, Checking, UpToDate, CheckFailed, Updates, Updating, Merging, Restart, Settings, Failed };
    enum class Action { None, Update, Restart, Merge, Check };
    Kind kind = Kind::NotChecked;
    QString stamp; // "checked 14:02", "updated 14:12", "tried 14:12"
    QString heading;
    QString detail;
    QString linkText; // "Read first: …"
    QString linkUrl;
    QString mergeText; // "1 settings file changed: /etc/sddm.conf" (with a Merge… button)
    QString failureKind; // Failed: the failure.h kind, for the sentence
    bool transactionStarted = false; // Failed: pacman had begun changing packages
    Action action = Action::None;
    QString actionText;
    bool primary = false; // the main thing to do (accent button) or just available

    bool operator==(const CardView &) const = default;
};

CardView cardView(const TrayInput &in);

// "/etc/sddm.conf" for "/etc/sddm.conf.pacnew" or ".pacsave".
QString configTarget(const QString &path);
}
