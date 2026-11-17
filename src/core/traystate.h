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
    bool sameBoot = false; // lastRun was written in this boot
    bool kernelReplaced = false; // the running kernel's modules are gone
    bool updating = false; // a kyu-update holds its lock
    bool otherManager = false; // pacman's lock is held, not by a kyu-update
    QDateTime now;
};

// Everything but the checker's state, read from the system now: kyu-update's
// last-run.json and lock, the boot, the running kernel, pacman's lock.
TrayInput currentTrayInput(const std::optional<UpdateState> &state, bool checking, const QString &loadError);

struct TrayView {
    TrayMark mark = TrayMark::UpToDate;
    QString iconName; // a theme icon (empty for Updating: Kyu's own frames)
    QString tooltip; // one or two lines
    bool passive = true; // nothing to do: the tray may hide it
};

TrayView trayView(const TrayInput &in);

// The card the tray opens: one heading, one line, at most one link and one
// extra row, one button.
struct CardView {
    enum class Action { None, Update, Restart, Merge, Check };
    QString stamp; // "checked 14:02", "updated 14:12"
    QString heading;
    QString detail;
    QString linkText; // "Read first: …"
    QString linkUrl;
    QString mergeText; // "1 settings file changed: /etc/sddm.conf" (with a Merge… button)
    QStringList configs; // the files behind mergeText
    QString failureKind; // for the "did not finish" detail (failure.h kinds)
    Action action = Action::None;
    QString actionText;
    bool primary = false; // the main thing to do (accent button) or just available
};

CardView cardView(const TrayInput &in);
}
