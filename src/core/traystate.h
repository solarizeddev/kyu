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
    QDateTime now;
};

struct TrayView {
    TrayMark mark = TrayMark::UpToDate;
    QString iconName; // a theme icon (empty for Updating: Kyu's own frames)
    QString tooltip; // one or two lines
    bool passive = true; // nothing to do: the tray may hide it
};

TrayView trayView(const TrayInput &in);
}
