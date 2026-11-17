#include "traystate.h"

#include "aftercare.h"
#include "news.h"
#include "paths.h"
#include "statustext.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
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

QString aurFailedLine(const QStringList &names)
{
    return QStringLiteral("AUR: %1 not updated last time; offered again with a newer version").arg(names.join(QStringLiteral(", ")));
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

void leaveOutFailedAur(TrayInput &in)
{
    if (in.state) {
        QSet<QString> failed;
        for (const QJsonValue &v : in.lastRun.value(QStringLiteral("aurFailed")).toArray()) {
            failed.insert(v[QStringLiteral("name")].toString() + QLatin1Char(' ') + v[QStringLiteral("version")].toString());
        }
        QList<PackageUpdate> &aur = in.state->aurPackages;
        for (auto it = aur.begin(); it != aur.end();) {
            if (failed.contains(it->name + QLatin1Char(' ') + it->newVersion)) {
                in.aurFailed.append(it->name);
                it = aur.erase(it);
            } else {
                ++it;
            }
        }
    }
}

TrayInput currentTrayInput(const std::optional<UpdateState> &state, bool checking, const QString &loadError)
{
    TrayInput in;
    in.state = state;
    in.checking = checking;
    in.loadError = loadError;
    in.lastRun = readJson(Paths::stateDir() + QStringLiteral("/last-run.json"));
    leaveOutFailedAur(in);
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
        if (!in.aurFailed.isEmpty()) {
            v.tooltip += QLatin1Char('\n') + aurFailedLine(in.aurFailed);
        }
    } else {
        v.mark = TrayMark::UpToDate;
        v.iconName = QStringLiteral("update-none");
        v.tooltip = status.join(QLatin1Char('\n'));
        if (!in.aurFailed.isEmpty()) {
            v.tooltip += QLatin1Char('\n') + aurFailedLine(in.aurFailed);
        }
        // A settings file left to merge keeps the icon in view.
        if (!in.configs.isEmpty()) {
            v.tooltip += QLatin1Char('\n') + settingsLine(in.configs);
        }
        v.passive = !in.checking && in.configs.isEmpty();
    }
    return v;
}

namespace
{
// "Updated 14:12": a word and when.
QString stamped(const QString &word, const QDateTime &when, const QDateTime &now)
{
    return when.isValid() ? word + QLatin1Char(' ') + shortWhen(when, now) : QString();
}

// What a check looks at: "repositories, AUR, Flatpak".
QString sources(const UpdateState &s)
{
    QStringList out;
    if (s.pacman.checked) {
        out.append(QStringLiteral("repositories"));
    }
    if (s.aur.checked) {
        out.append(QStringLiteral("AUR"));
    }
    if (s.flatpak.checked) {
        out.append(QStringLiteral("Flatpak"));
    }
    return out.isEmpty() ? QStringLiteral("repositories") : out.join(QStringLiteral(", "));
}

// A failed check in one plain sentence; the program's own words go to Details.
QString checkFailure(const UpdateState &s)
{
    for (const auto &[status, name] : {std::pair{&s.pacman, QStringLiteral("pacman")}, std::pair{&s.flatpak, QStringLiteral("Flatpak")}}) {
        if (!status->checked || status->ok) {
            continue;
        }
        const QString e = status->error.toLower();
        for (const char *n : {"resolve", "could not connect", "couldn't connect", "failed to connect", "network is unreachable", "connection refused"}) {
            if (e.contains(QLatin1String(n))) {
                return QStringLiteral("No network connection.");
            }
        }
        if (e.contains(QLatin1String("timed out")) || e.contains(QLatin1String("timeout"))) {
            return name == QLatin1String("Flatpak") ? QStringLiteral("Flatpak's server did not answer.") : QStringLiteral("The mirrors did not answer.");
        }
        return QStringLiteral("%1 reported an error.").arg(name);
    }
    return QStringLiteral("The check did not finish.");
}

// The failed sources' own messages, for Details.
QString checkErrors(const UpdateState &s)
{
    QStringList out;
    for (const auto &[status, name] : {std::pair{&s.pacman, QStringLiteral("pacman")}, std::pair{&s.flatpak, QStringLiteral("Flatpak")},
                                       std::pair{&s.aur, QStringLiteral("AUR")}}) {
        if (status->checked && !status->ok && !status->error.isEmpty()) {
            out.append(name + QStringLiteral(": ") + status->error);
        }
    }
    return out.join(QLatin1Char('\n'));
}

// The last successful check of the sources that failed.
QDateTime lastSuccess(const UpdateState &s)
{
    QDateTime when;
    for (const SourceStatus *status : {&s.pacman, &s.flatpak}) {
        if (status->checked && !status->ok && status->lastSuccessAt.isValid() && (!when.isValid() || status->lastSuccessAt < when)) {
            when = status->lastSuccessAt;
        }
    }
    return when;
}

QList<DetailRow> detailRows(const UpdateState &s)
{
    QList<DetailRow> first, rest;
    const QString arrow = QStringLiteral(" → ");
    for (const PackageUpdate &p : s.packages) {
        (p.restart ? first : rest).append({p.name, p.oldVersion + arrow + p.newVersion, p.repo, p.restart});
    }
    for (const PackageUpdate &p : s.aurPackages) {
        rest.append({p.name, p.oldVersion + arrow + p.newVersion, QStringLiteral("AUR"), false});
    }
    for (const FlatpakUpdate &f : s.flatpaks) {
        rest.append({f.name, f.branch, QStringLiteral("Flatpak"), false});
    }
    return first + rest;
}

// "1 settings file to merge: /etc/sddm.conf", "3 settings files to merge".
QString toMerge(const QStringList &configs, const QString &tail = QString())
{
    return configs.size() == 1 ? QStringLiteral("1 settings file to merge") + tail + QStringLiteral(": ") + configTarget(configs.constFirst())
                               : QStringLiteral("%1 settings files to merge").arg(configs.size()) + tail;
}
}

CardView cardView(const TrayInput &in)
{
    CardView c;
    const TrayView tray = trayView(in);
    const QDateTime lastTime = lastRunTime(in.lastRun);
    const int total = in.state ? in.state->total() : 0;
    const QString checked = in.state ? stamped(QStringLiteral("Checked"), in.state->checkedAt, in.now) : QString();
    // A check running over what is known: that card, "Checking…" in its corner.
    const auto corner = [&in](const QString &stamp) {
        return in.checking ? QStringLiteral("Checking…") : stamp;
    };

    switch (tray.mark) {
    case TrayMark::Updating:
        if (in.activity == QLatin1String("merge")) {
            c.kind = CardView::Kind::Merging;
            c.icon = QStringLiteral("merge");
            c.heading = QStringLiteral("Merging settings files…");
            c.detail = QStringLiteral("pacdiff is running in Kyu's window.");
        } else {
            c.kind = CardView::Kind::Updating;
            c.icon = QStringLiteral("system-upgrade");
            c.heading = QStringLiteral("Updating…");
            c.detail = QStringLiteral("The update is running in Kyu's window.");
        }
        c.busy = true;
        c.action = CardView::Action::ShowWindow;
        c.actionText = QStringLiteral("Show window");
        c.primary = true;
        return c;
    case TrayMark::Error:
        if (tray.lastFailed) {
            c.kind = CardView::Kind::Failed;
            c.icon = QStringLiteral("update-high");
            c.stamp = stamped(QStringLiteral("Tried"), lastTime, in.now);
            c.heading = QStringLiteral("The update did not finish");
            c.failureKind = in.lastRun.value(QStringLiteral("failure")).toString();
            c.transactionStarted = in.lastRun.value(QStringLiteral("transactionStarted")).toBool();
            c.changed = in.lastRun.value(QStringLiteral("changed")).toInt();
            c.detail = c.transactionStarted ? QStringLiteral("pacman stopped halfway.")
                : c.changed > 0             ? QStringLiteral("Only ") + plural(c.changed, QStringLiteral("package was updated."), QStringLiteral("packages were updated."))
                                            : QStringLiteral("Nothing was installed.");
            QStringList lines;
            for (const QJsonValue &v : in.lastRun.value(QStringLiteral("failureLines")).toArray()) {
                lines.append(v.toString());
            }
            c.raw = lines.join(QLatin1Char('\n'));
            if (!in.configs.isEmpty()) {
                c.note = toMerge(in.configs, QStringLiteral(" after the update"));
            }
            c.canRefresh = !in.checking;
            c.action = CardView::Action::Update;
            c.actionText = QStringLiteral("Update again");
            c.primary = true;
        } else {
            c.kind = CardView::Kind::CheckFailed;
            c.icon = QStringLiteral("data-warning");
            c.heading = QStringLiteral("Could not check");
            if (!in.loadError.isEmpty() || !in.state) {
                c.detail = QStringLiteral("Kyu could not read its list of updates.");
                c.raw = in.loadError;
            } else {
                c.detail = checkFailure(*in.state);
                if (const QDateTime ok = lastSuccess(*in.state); ok.isValid()) {
                    c.detail += QStringLiteral("\nLast successful check: ") + shortWhen(ok, in.now) + QLatin1Char('.');
                }
                c.raw = checkErrors(*in.state);
                c.stamp = corner(stamped(QStringLiteral("Tried"), in.state->checkedAt, in.now));
            }
            if (!in.checking) {
                c.action = CardView::Action::Check;
                c.actionText = QStringLiteral("Try again");
                c.primary = true;
            }
        }
        return c;
    case TrayMark::Restart: {
        c.kind = CardView::Kind::Restart;
        c.icon = QStringLiteral("system-reboot-update");
        if (lastTime.isValid() && in.sameBoot) {
            c.stamp = stamped(QStringLiteral("Updated"), lastTime, in.now);
        }
        c.heading = QStringLiteral("Restart to finish");
        // "new kernel 6.18.22-1-MANJARO", "the running kernel 6.18.20-1 was
        // removed", "systemd was updated" (a package name keeps its case).
        QString reason = restartReason(in);
        reason.remove(QStringLiteral(" (from an earlier update)"));
        static const QRegularExpression updatedPackage(QStringLiteral("^(\\S+) was updated$"));
        if (reason.startsWith(QLatin1String("new kernel "))) {
            c.detail = QStringLiteral("Kernel %1 is installed.\nThe old one runs until you restart.").arg(reason.mid(11));
        } else if (reason.startsWith(QLatin1String("the running kernel"))) {
            reason[0] = reason[0].toUpper();
            c.detail = reason + QStringLiteral(".\nRestart to use the installed one.");
        } else if (const auto m = updatedPackage.match(reason); m.hasMatch()) {
            c.detail = QStringLiteral("An update of %1 is installed.\nThe old version runs until you restart.").arg(m.captured(1));
        } else {
            if (reason.startsWith(QLatin1String("the "))) {
                reason[0] = QLatin1Char('T');
            }
            c.detail = reason + QStringLiteral(".\nThe old version runs until you restart.");
        }
        if (!in.configs.isEmpty()) {
            c.note = toMerge(in.configs);
            c.secondary = CardView::Action::Merge;
            c.secondaryText = QStringLiteral("Merge…");
        }
        c.action = CardView::Action::Restart;
        c.actionText = QStringLiteral("Restart");
        c.primary = true;
        return c;
    }
    case TrayMark::News:
    case TrayMark::Updates: {
        const UpdateState &s = *in.state;
        c.kind = CardView::Kind::Updates;
        const NewsItem *unread = unreadNews(in);
        c.icon = unread ? QStringLiteral("update-medium") : QStringLiteral("update-low");
        c.heading = plural(total, QStringLiteral("update"), QStringLiteral("updates"));
        QStringList what;
        if (!s.packages.isEmpty()) {
            what.append(plural(s.packages.size(), QStringLiteral("package"), QStringLiteral("packages")));
        }
        if (!s.aurPackages.isEmpty()) {
            what.append(QStringLiteral("%1 AUR").arg(s.aurPackages.size()));
        }
        if (!s.flatpaks.isEmpty()) {
            what.append(plural(s.flatpaks.size(), QStringLiteral("Flatpak"), QStringLiteral("Flatpaks")));
        }
        QString first = what.join(QStringLiteral(", "));
        if (s.totalSize() > 0) {
            first += QStringLiteral(" · ") + QLocale().formattedDataSize(s.totalSize(), 0);
        }
        if (!in.aurFailed.isEmpty()) {
            first += QStringLiteral(" · %1 AUR held back").arg(in.aurFailed.size());
        }
        // A failed check keeps the last list: said, with its age.
        const bool stale = s.hasError() || !in.loadError.isEmpty();
        QString second;
        if (stale) {
            if (s.restartNeeded()) {
                first += QStringLiteral(" · needs a restart");
            }
            // When it failed is in the corner ("Tried 14:02").
            const QDateTime ok = lastSuccess(s);
            second = ok.isValid() ? QStringLiteral("Check failed; this list is from %1.").arg(shortWhen(ok, in.now))
                                  : QStringLiteral("Check failed; this is the last list.");
            c.raw = checkErrors(s);
            c.stamp = corner(stamped(QStringLiteral("Tried"), s.checkedAt, in.now));
        } else {
            if (s.restartNeeded()) {
                second = QStringLiteral("Needs a restart afterwards.");
            } else if (s.packages.isEmpty() && s.aurPackages.isEmpty()) {
                second = QStringLiteral("No password needed.");
            }
            c.stamp = corner(checked);
        }
        c.detail = second.isEmpty() ? first : first + QLatin1Char('\n') + second;
        c.canRefresh = !in.checking;
        // The news: "Read first" until read (confirmed in kyu-update, or
        // opened from here); afterwards just the link.
        if (unread) {
            c.linkText = QStringLiteral("Read first: ") + unread->title;
            c.linkUrl = unread->link;
            c.linkKey = newsKey(*unread);
        } else if (!s.news.items.isEmpty()) {
            c.linkText = QStringLiteral("News: ") + s.news.items.constFirst().title;
            c.linkUrl = s.news.items.constFirst().link;
            c.linkKey = newsKey(s.news.items.constFirst());
        }
        c.rows = detailRows(s);
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
        c.icon = QStringLiteral("merge");
        c.stamp = corner(stamped(QStringLiteral("Updated"), lastTime, in.now));
        c.canRefresh = !in.checking;
        c.heading = in.configs.size() == 1 ? QStringLiteral("1 settings file to merge") : QStringLiteral("%1 settings files to merge").arg(in.configs.size());
        if (in.configs.size() > 1) {
            c.detail = QStringLiteral("New versions came with the update.\nYours stay in use until you merge.");
        } else if (const QString &file = in.configs.constFirst(); file.endsWith(QLatin1String(".pacsave"))) {
            c.detail = QStringLiteral("%1 was replaced by the package's version.\nYours is kept as %2.").arg(configTarget(file), QFileInfo(file).fileName());
        } else {
            c.detail = QStringLiteral("%1 has a new version.\nYours stays in use until you merge.").arg(configTarget(file));
        }
        c.action = CardView::Action::Merge;
        c.actionText = QStringLiteral("Merge…");
        c.primary = true;
        return c;
    }
    if (!in.state) {
        c.kind = in.checking ? CardView::Kind::Checking : CardView::Kind::NotChecked;
        c.icon = QStringLiteral("system-upgrade");
        c.heading = in.checking ? QStringLiteral("Checking for updates…") : QStringLiteral("Not checked yet");
        c.detail = in.checking ? QStringLiteral("Asking the repositories, AUR and Flatpak.") : QStringLiteral("Kyu has not looked for updates yet.");
        c.busy = in.checking;
        if (!in.checking) {
            c.action = CardView::Action::Check;
            c.actionText = QStringLiteral("Check now");
            c.primary = true;
        }
        return c;
    }
    c.kind = CardView::Kind::UpToDate;
    c.icon = QStringLiteral("checkmark");
    c.heading = QStringLiteral("Up to date");
    c.detail = QStringLiteral("Nothing new: %1.").arg(sources(*in.state));
    if (lastTime.isValid()) {
        c.detail += QStringLiteral("\nLast updated ") + shortWhen(lastTime, in.now) + QLatin1Char('.');
    }
    if (!in.aurFailed.isEmpty()) {
        c.note = aurFailedLine(in.aurFailed);
    }
    c.stamp = corner(checked);
    if (!in.checking) {
        c.action = CardView::Action::Check;
        c.actionText = QStringLiteral("Check now");
    }
    return c;
}
}
