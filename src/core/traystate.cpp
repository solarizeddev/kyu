#include "traystate.h"

#include "aftercare.h"
#include "paths.h"
#include "statustext.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QLockFile>
#include <QRegularExpression>

#include <csignal>

namespace Kyu
{
TrayView trayView(const TrayInput &in)
{
    TrayView v;
    v.passive = false;
    const QJsonObject &last = in.lastRun;
    const QDateTime lastTime = QDateTime::fromString(last.value(QStringLiteral("time")).toString(), Qt::ISODate);
    // A failed update stays until a later check finds the system up to date.
    const bool lastFailed = last.value(QStringLiteral("pacmanExit")).toInt(0) > 0
        && !(in.state && in.state->checkedAt.isValid() && lastTime.isValid() && in.state->checkedAt > lastTime && in.state->total() == 0);
    const bool checkFailed = !in.loadError.isEmpty() || (in.state && in.state->hasError());
    const bool restart = in.kernelReplaced || (in.sameBoot && last.value(QStringLiteral("restartRequired")).toBool());
    const int total = in.state ? in.state->total() : 0;
    const bool news = in.state && total > 0 && !in.state->news.items.isEmpty();
    const QStringList status = statusLines(in.state, in.checking, in.loadError, in.now);

    if (in.updating) {
        v.mark = TrayMark::Updating;
        v.tooltip = QStringLiteral("Updating");
    } else if (lastFailed) {
        v.mark = TrayMark::Error;
        v.iconName = QStringLiteral("update-high");
        v.tooltip = QStringLiteral("The last update did not finish");
    } else if (checkFailed) {
        v.mark = TrayMark::Error;
        v.iconName = QStringLiteral("update-high");
        v.tooltip = status.join(QLatin1Char('\n'));
    } else if (restart) {
        v.mark = TrayMark::Restart;
        v.iconName = QStringLiteral("system-reboot-update");
        const QString reason = last.value(QStringLiteral("restartReason")).toString();
        v.tooltip = QStringLiteral("Restart to finish") + (in.sameBoot && !reason.isEmpty() ? QStringLiteral(": ") + reason : QString());
    } else if (news) {
        v.mark = TrayMark::News;
        v.iconName = QStringLiteral("update-medium");
        v.tooltip = QStringLiteral("Read first: ") + in.state->news.items.constFirst().title + QLatin1Char('\n') + status.value(0);
    } else if (total > 0) {
        v.mark = TrayMark::Updates;
        v.iconName = QStringLiteral("update-low");
        v.tooltip = status.join(QLatin1Char('\n'));
    } else {
        v.mark = TrayMark::UpToDate;
        v.iconName = QStringLiteral("update-none");
        v.tooltip = status.join(QLatin1Char('\n'));
        v.passive = !in.checking;
    }
    return v;
}
}

namespace Kyu
{
namespace
{
QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

QString bootId()
{
    QFile file(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QString plural(qsizetype n, const QString &one, const QString &many)
{
    return n == 1 ? QStringLiteral("1 ") + one : QStringLiteral("%1 ").arg(n) + many;
}

QString clock(const QDateTime &t)
{
    return QLocale().toString(t.toLocalTime().time(), QLocale::ShortFormat);
}
}

TrayInput currentTrayInput(const std::optional<UpdateState> &state, bool checking, const QString &loadError)
{
    TrayInput in;
    in.state = state;
    in.checking = checking;
    in.loadError = loadError;
    in.lastRun = readJson(Paths::stateDir() + QStringLiteral("/last-run.json"));
    const QString boot = bootId();
    in.sameBoot = !boot.isEmpty() && in.lastRun.value(QStringLiteral("bootId")).toString() == boot;
    in.kernelReplaced = kernelState().replaced;
    // Read only: taking the lock would make a starting kyu-update think
    // another one runs.
    QLockFile lock(Paths::stateDir() + QStringLiteral("/update.lock"));
    qint64 pid = 0;
    QString host, app;
    in.updating = lock.getLockInfo(&pid, &host, &app) && pid > 0 && ::kill(pid_t(pid), 0) == 0;
    in.otherManager = !in.updating && QFileInfo::exists(QStringLiteral("/var/lib/pacman/db.lck"));
    in.now = QDateTime::currentDateTime();
    return in;
}

CardView cardView(const TrayInput &in)
{
    CardView c;
    const TrayView tray = trayView(in);
    const QJsonObject &last = in.lastRun;
    const QDateTime lastTime = QDateTime::fromString(last.value(QStringLiteral("time")).toString(), Qt::ISODate);
    if (in.state && in.state->checkedAt.isValid()) {
        c.stamp = QStringLiteral("checked ") + clock(in.state->checkedAt);
    }
    // Settings files from the last update that are still waiting.
    for (const QJsonValue &v : last.value(QStringLiteral("configs")).toArray()) {
        if (QFileInfo::exists(v.toString())) {
            c.configs.append(v.toString());
        }
    }
    if (!c.configs.isEmpty()) {
        QString shown = c.configs.constFirst();
        shown.remove(QRegularExpression(QStringLiteral("\\.pac(new|save)$")));
        c.mergeText = c.configs.size() == 1 ? QStringLiteral("1 settings file changed: ") + shown
                                            : QStringLiteral("%1 settings files changed").arg(c.configs.size());
    }
    auto check = [&c]() {
        c.action = CardView::Action::Check;
        c.actionText = QStringLiteral("Check for updates");
    };

    switch (tray.mark) {
    case TrayMark::Updating:
        c.heading = QStringLiteral("Updating…");
        c.detail = QStringLiteral("In the Kyu window.");
        c.mergeText.clear();
        return c;
    case TrayMark::Error:
        if (tray.tooltip == QLatin1String("The last update did not finish")) {
            c.stamp = lastTime.isValid() ? QStringLiteral("updated ") + clock(lastTime) : QString();
            c.heading = QStringLiteral("The update did not finish");
            c.failureKind = last.value(QStringLiteral("failure")).toString();
            c.action = CardView::Action::Update;
            c.actionText = QStringLiteral("Update again");
            c.primary = true;
        } else {
            c.heading = QStringLiteral("Could not check");
            c.detail = tray.tooltip.section(QLatin1Char('\n'), -1);
            check();
        }
        return c;
    case TrayMark::Restart:
        if (lastTime.isValid() && in.sameBoot) {
            c.stamp = QStringLiteral("updated ") + clock(lastTime);
        }
        c.heading = QStringLiteral("Restart to finish");
        {
            QString reason = in.sameBoot ? last.value(QStringLiteral("restartReason")).toString() : QString();
            if (reason.isEmpty()) {
                reason = QStringLiteral("a new kernel is installed");
            }
            reason[0] = reason[0].toUpper();
            c.detail = reason;
        }
        c.action = CardView::Action::Restart;
        c.actionText = QStringLiteral("Restart");
        c.primary = true;
        return c;
    case TrayMark::News:
    case TrayMark::Updates: {
        const UpdateState &s = *in.state;
        QStringList parts;
        if (!s.packages.isEmpty()) {
            parts.append(plural(s.packages.size(), QStringLiteral("package"), QStringLiteral("packages")));
        }
        if (!s.flatpaks.isEmpty()) {
            parts.append(plural(s.flatpaks.size(), QStringLiteral("Flatpak"), QStringLiteral("Flatpaks")));
        }
        if (s.packages.isEmpty()) {
            parts.append(QStringLiteral("no password needed"));
        } else if (s.totalSize() > 0) {
            parts.append(QLocale().formattedDataSize(s.totalSize(), 0));
        }
        if (s.restartNeeded()) {
            parts.append(QStringLiteral("restart after"));
        }
        c.heading = plural(s.total(), QStringLiteral("update"), QStringLiteral("updates"));
        c.detail = parts.join(QStringLiteral(" · "));
        if (!s.news.items.isEmpty()) {
            c.linkText = QStringLiteral("Read first: ") + s.news.items.constFirst().title;
            c.linkUrl = s.news.items.constFirst().link;
        }
        if (in.otherManager) {
            c.detail += QStringLiteral(" · another package manager is running");
        }
        c.action = CardView::Action::Update;
        c.actionText = QStringLiteral("Update");
        c.primary = true;
        return c;
    }
    case TrayMark::UpToDate:
        break;
    }
    // Up to date: a settings file may still be waiting.
    if (!c.configs.isEmpty()) {
        c.stamp = lastTime.isValid() ? QStringLiteral("updated ") + clock(lastTime) : c.stamp;
        c.heading = c.configs.size() == 1 ? QStringLiteral("1 settings file changed") : QStringLiteral("%1 settings files changed").arg(c.configs.size());
        QString shown = c.configs.constFirst();
        shown.remove(QRegularExpression(QStringLiteral("\\.pac(new|save)$")));
        c.detail = c.configs.size() == 1 ? shown + QStringLiteral(" has a new version") : QStringLiteral("New versions to compare and merge");
        c.mergeText.clear();
        c.action = CardView::Action::Merge;
        c.actionText = QStringLiteral("Merge…");
        c.primary = true;
        return c;
    }
    if (!in.state) {
        c.heading = in.checking ? QStringLiteral("Checking…") : QStringLiteral("Not checked yet");
        if (!in.checking) {
            check();
        }
        return c;
    }
    c.heading = QStringLiteral("Up to date");
    c.detail = in.checking ? QStringLiteral("Checking…") : QString();
    if (!in.checking) {
        check();
    }
    return c;
}
}
