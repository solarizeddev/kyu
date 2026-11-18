#pragma once

#include "traystate.h"

#include <QDateTime>
#include <QString>

namespace Kyu
{
// The tray's "Updates" notification (kyu.notifyrc, "updates"): once for each
// list that follows an empty one, at most once in MinHoursBetween hours;
// never when a list already told only grows or shrinks. Held back (not
// dropped) while news is unread, while a check runs and within the
// MinHoursBetween hours: it comes once that is over, if the updates are
// still there. It goes away by itself, and is closed when an update starts
// or the list is empty.
struct UpdatesMemory {
    bool told = false; // this list (since the last empty one) was notified
    QDateTime sentAt; // the last notification (UTC)

    bool operator==(const UpdatesMemory &) const = default;
};

struct UpdatesNotice {
    enum class Do { Nothing, Send, Close };
    Do action = Do::Nothing;
    QString title; // "8 updates available"
    QString text; // "6 packages, 2 Flatpaks · including the kernel and Plasma"

    bool operator==(const UpdatesNotice &) const = default;
};

constexpr int MinHoursBetween = 20;

// What to do now, given what the tray reads; memory is updated (kept across
// restarts of the tray, in updates-notified.json).
UpdatesNotice updatesNotice(const TrayInput &in, UpdatesMemory &memory);

// The packages worth naming, at most these, in this order: the kernel,
// Plasma, Mesa, the NVIDIA driver, systemd.
QStringList notableUpdates(const UpdateState &state);

UpdatesMemory readUpdatesMemory(const QString &path);
bool writeUpdatesMemory(const UpdatesMemory &memory, const QString &path);
}
