#include "traystate.h"

#include "aftercare.h"
#include "news.h"
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
namespace
{
QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

QString readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QString plural(qsizetype n, const QString &one, const QString &many)
{
    return n == 1 ? QStringLiteral("1 ") + one : QStringLiteral("%1 ").arg(n) + many;
}

QDateTime lastRunTime(const QJsonObject &last)
{
    return QDateTime::fromString(last.value(QStringLiteral("time")).toString(), Qt::ISODate);
}

// The last update failed, and no later check has found the system up to date.
bool lastRunFailed(const TrayInput &in)
{
    const QDateTime lastTime = lastRunTime(in.lastRun);
    return in.lastRun.value(QStringLiteral("pacmanExit")).toInt(0) > 0
        && !(in.state && in.state->checkedAt.isValid() && lastTime.isValid() && in.state->checkedAt > lastTime && in.state->total() == 0);
}

// Why a restart is needed, as kyu-update says it ("new kernel 6.18.22-1-MANJARO").
QString restartReason(const TrayInput &in)
{
    const QString recorded = in.lastRun.value(QStringLiteral("restartReason")).toString();
    if (in.sameBoot && !recorded.isEmpty()) {
        return recorded;
    }
    return in.kernelInstalled.isEmpty() ? QStringLiteral("the running kernel was replaced") : QStringLiteral("new kernel ") + in.kernelInstalled;
}

QString settingsLine(const QStringList &configs)
{
    return configs.size() == 1 ? QStringLiteral("1 settings file changed: ") + configTarget(configs.constFirst())
                               : QStringLiteral("%1 settings files changed").arg(configs.size());
}
}

QString configTarget(const QString &path)
{
    static const QRegularExpression suffix(QStringLiteral("\\.pac(new|save)$"));
    return QString(path).remove(suffix);
}

TrayInput currentTrayInput(const std::optional<UpdateState> &state, bool checking, const QString &loadError)
{
    TrayInput in;
    in.state = state;
    in.checking = checking;
    in.loadError = loadError;
    in.lastRun = readJson(Paths::stateDir() + QStringLiteral("/last-run.json"));
    for (const QJsonValue &v : in.lastRun.value(QStringLiteral("configs")).toArray()) {
        if (QFileInfo::exists(v.toString())) {
            in.configs.append(v.toString());
        }
    }
    const QString boot = readText(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    in.sameBoot = !boot.isEmpty() && in.lastRun.value(QStringLiteral("bootId")).toString() == boot;
    const KernelState kernel = kernelState();
    in.kernelReplaced = kernel.replaced;
    in.kernelInstalled = kernel.installed;
    // Read only: taking the lock would make a starting kyu-update think
    // another one runs.
    QLockFile lock(Paths::stateDir() + QStringLiteral("/update.lock"));
    qint64 pid = 0;
    QString host, app;
    in.updating = lock.getLockInfo(&pid, &host, &app) && pid > 0 && ::kill(pid_t(pid), 0) == 0;
    if (in.updating) {
        in.activity = readText(Paths::stateDir() + QStringLiteral("/activity"));
    }
    in.newsSeen = seenNews();
    in.now = QDateTime::currentDateTime();
    return in;
}

namespace
{
// The first announcement not read yet, if any.
const NewsItem *unreadNews(const TrayInput &in)
{
    if (!in.state) {
        return nullptr;
    }
    for (const NewsItem &item : in.state->news.items) {
        if (!in.newsSeen.contains(newsKey(item))) {
            return &item;
        }
    }
    return nullptr;
}
}

TrayView trayView(const TrayInput &in)
{
    TrayView v;
    v.passive = false;
    const bool lastFailed = lastRunFailed(in);
    const bool restart = in.kernelReplaced || (in.sameBoot && in.lastRun.value(QStringLiteral("restartRequired")).toBool());
    const int total = in.state ? in.state->total() : 0;
    // A failed check with updates still listed shows the updates (the tooltip
    // says the check failed); with none listed it is the state to show.
    const bool checkFailed = total == 0 && (!in.loadError.isEmpty() || (in.state && in.state->hasError()));
    const NewsItem *unread = unreadNews(in);
    const bool news = total > 0 && unread;
    const QStringList status = statusLines(in.state, in.checking, in.loadError, in.now);

    if (in.updating) {
        v.mark = TrayMark::Updating;
        v.tooltip = in.activity == QLatin1String("merge") ? QStringLiteral("Merging settings files") : QStringLiteral("Updating");
    } else if (lastFailed) {
        v.mark = TrayMark::Error;
        v.iconName = QStringLiteral("update-high");
        v.tooltip = QStringLiteral("The last update did not finish");
        v.lastFailed = true;
    } else if (checkFailed) {
        v.mark = TrayMark::Error;
        v.iconName = QStringLiteral("update-high");
        v.tooltip = status.join(QLatin1Char('\n'));
    } else if (restart) {
        v.mark = TrayMark::Restart;
        v.iconName = QStringLiteral("system-reboot-update");
        v.tooltip = QStringLiteral("Restart to finish: ") + restartReason(in);
    } else if (news) {
        v.mark = TrayMark::News;
        v.iconName = QStringLiteral("update-medium");
        v.tooltip = QStringLiteral("Read first: ") + unread->title + QLatin1Char('\n') + status.value(0);
        if (const QString failed = failureLines(*in.state, in.now).value(0); !failed.isEmpty()) {
            v.tooltip += QLatin1Char('\n') + failed;
        }
    } else if (total > 0) {
        v.mark = TrayMark::Updates;
        v.iconName = QStringLiteral("update-low");
        v.tooltip = status.join(QLatin1Char('\n'));
    } else {
        v.mark = TrayMark::UpToDate;
        v.iconName = QStringLiteral("update-none");
        v.tooltip = status.join(QLatin1Char('\n'));
        // A settings file left to merge keeps the icon in view.
        if (!in.configs.isEmpty()) {
            v.tooltip += QLatin1Char('\n') + settingsLine(in.configs);
        }
        v.passive = !in.checking && in.configs.isEmpty();
    }
    return v;
}

CardView cardView(const TrayInput &in)
{
    CardView c;
    const TrayView tray = trayView(in);
    const QDateTime lastTime = lastRunTime(in.lastRun);
    const int total = in.state ? in.state->total() : 0;
    if (in.state && in.state->checkedAt.isValid()) {
        c.stamp = QStringLiteral("checked ") + shortWhen(in.state->checkedAt, in.now);
    }
    if (!in.configs.isEmpty()) {
        c.mergeText = settingsLine(in.configs);
    }
    auto check = [&c, &in]() {
        if (!in.checking) {
            c.action = CardView::Action::Check;
            c.actionText = QStringLiteral("Check for updates");
        }
    };

    switch (tray.mark) {
    case TrayMark::Updating:
        if (in.activity == QLatin1String("merge")) {
            c.kind = CardView::Kind::Merging;
            c.heading = QStringLiteral("Merging settings files…");
        } else {
            c.kind = CardView::Kind::Updating;
            c.heading = QStringLiteral("Updating…");
        }
        c.detail = QStringLiteral("In the Kyu window.");
        c.stamp.clear();
        c.mergeText.clear();
        return c;
    case TrayMark::Error:
        if (tray.lastFailed) {
            c.kind = CardView::Kind::Failed;
            c.stamp = lastTime.isValid() ? QStringLiteral("tried ") + shortWhen(lastTime, in.now) : QString();
            c.heading = QStringLiteral("The update did not finish");
            c.failureKind = in.lastRun.value(QStringLiteral("failure")).toString();
            c.transactionStarted = in.lastRun.value(QStringLiteral("transactionStarted")).toBool();
            c.changed = in.lastRun.value(QStringLiteral("changed")).toInt();
            c.action = CardView::Action::Update;
            c.actionText = QStringLiteral("Update again");
            c.primary = true;
        } else {
            c.kind = CardView::Kind::CheckFailed;
            c.heading = QStringLiteral("Could not check");
            QString why = in.loadError;
            if (why.isEmpty() && in.state) {
                why = failureLines(*in.state, in.now).value(0);
            }
            c.detail = why;
            check();
        }
        return c;
    case TrayMark::Restart:
        c.kind = CardView::Kind::Restart;
        if (lastTime.isValid() && in.sameBoot) {
            c.stamp = QStringLiteral("updated ") + shortWhen(lastTime, in.now);
        }
        c.heading = QStringLiteral("Restart to finish");
        c.detail = restartReason(in);
        c.detail[0] = c.detail[0].toUpper();
        c.action = CardView::Action::Restart;
        c.actionText = QStringLiteral("Restart");
        c.primary = true;
        return c;
    case TrayMark::News:
    case TrayMark::Updates: {
        const UpdateState &s = *in.state;
        c.kind = CardView::Kind::Updates;
        QStringList parts;
        if (!s.packages.isEmpty()) {
            parts.append(plural(s.packages.size(), QStringLiteral("package"), QStringLiteral("packages")));
        }
        if (!s.aurPackages.isEmpty()) {
            parts.append(QStringLiteral("%1 AUR").arg(s.aurPackages.size()));
        }
        if (!s.flatpaks.isEmpty()) {
            parts.append(plural(s.flatpaks.size(), QStringLiteral("Flatpak"), QStringLiteral("Flatpaks")));
        }
        if (s.packages.isEmpty() && s.aurPackages.isEmpty()) {
            parts.append(QStringLiteral("no password needed"));
        } else if (s.totalSize() > 0) {
            parts.append(QLocale().formattedDataSize(s.totalSize(), 0));
        }
        if (s.restartNeeded()) {
            parts.append(QStringLiteral("restart after"));
        }
        if (s.hasError() || !in.loadError.isEmpty()) {
            parts.append(QStringLiteral("a check failed"));
        }
        c.heading = plural(total, QStringLiteral("update"), QStringLiteral("updates"));
        c.detail = parts.join(QStringLiteral(" · "));
        // The news: "Read first" until read (confirmed in kyu-update, or
        // opened from here); afterwards just the link.
        if (const NewsItem *unread = unreadNews(in)) {
            c.linkText = QStringLiteral("Read first: ") + unread->title;
            c.linkUrl = unread->link;
            c.linkKey = newsKey(*unread);
        } else if (!s.news.items.isEmpty()) {
            c.linkText = QStringLiteral("News: ") + s.news.items.constFirst().title;
            c.linkUrl = s.news.items.constFirst().link;
            c.linkKey = newsKey(s.news.items.constFirst());
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
    if (!in.configs.isEmpty()) {
        c.kind = CardView::Kind::Settings;
        if (lastTime.isValid()) {
            c.stamp = QStringLiteral("updated ") + shortWhen(lastTime, in.now);
        }
        c.heading = in.configs.size() == 1 ? QStringLiteral("1 settings file changed") : QStringLiteral("%1 settings files changed").arg(in.configs.size());
        c.detail = in.configs.size() == 1 ? configTarget(in.configs.constFirst()) + QStringLiteral(" has a new version")
                                          : QStringLiteral("New versions to compare and merge");
        c.mergeText.clear();
        c.action = CardView::Action::Merge;
        c.actionText = QStringLiteral("Merge…");
        c.primary = true;
        return c;
    }
    if (!in.state) {
        c.kind = in.checking ? CardView::Kind::Checking : CardView::Kind::NotChecked;
        c.heading = in.checking ? QStringLiteral("Checking…") : QStringLiteral("Not checked yet");
        check();
        return c;
    }
    c.kind = CardView::Kind::UpToDate;
    c.heading = QStringLiteral("Up to date");
    c.detail = in.checking ? QStringLiteral("Checking…") : QString();
    check();
    return c;
}
}
