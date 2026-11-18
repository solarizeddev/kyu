#include "plan.h"

#include "i18n.h"
#include "paths.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

namespace Kyu
{
namespace
{
const QHash<QString, QueueItem::Kind> &kindNames()
{
    static const QHash<QString, QueueItem::Kind> names{{QStringLiteral("install"), QueueItem::Kind::Install},
                                                       {QStringLiteral("reinstall"), QueueItem::Kind::Reinstall},
                                                       {QStringLiteral("remove"), QueueItem::Kind::Remove},
                                                       {QStringLiteral("file"), QueueItem::Kind::File},
                                                       {QStringLiteral("reason"), QueueItem::Kind::Reason}};
    return names;
}

// A package name as pacman allows it; anything else never reaches a command.
bool validName(const QString &name)
{
    static const QRegularExpression valid(QStringLiteral("^[a-z0-9@_+][a-z0-9@._+-]*$"));
    return name.size() < 256 && valid.match(name).hasMatch();
}

QStringList lines(const QByteArray &output)
{
    QStringList result;
    for (const QByteArray &line : output.split('\n')) {
        const QString l = QString::fromUtf8(line).trimmed();
        if (!l.isEmpty()) {
            result.append(l);
        }
    }
    return result;
}

// pacman's last error line, as said (the dry run is in the C locale).
QString lastError(const CommandResult &r)
{
    QStringList all = lines(r.err) + lines(r.out);
    for (auto it = all.crbegin(); it != all.crend(); ++it) {
        if (it->startsWith(QLatin1String("error: "))) {
            return it->mid(7);
        }
    }
    return all.isEmpty() ? QString() : all.constLast();
}

// pacman's version order (alpm_pkg_vercmp), for "upgrade" or "downgrade".
int vercmp(const QString &a, const QString &b)
{
    if (a == b) {
        return 0;
    }
    auto split = [](const QString &v, QString &epoch, QString &ver, QString &rel) {
        const qsizetype colon = v.indexOf(QLatin1Char(':'));
        epoch = colon >= 0 ? v.left(colon) : QStringLiteral("0");
        QString rest = colon >= 0 ? v.mid(colon + 1) : v;
        const qsizetype dash = rest.lastIndexOf(QLatin1Char('-'));
        ver = dash >= 0 ? rest.left(dash) : rest;
        rel = dash >= 0 ? rest.mid(dash + 1) : QString();
    };
    // rpmvercmp: runs of digits compare as numbers, letters as text,
    // digits beat letters, a longer remainder wins.
    auto rpm = [](const QString &x, const QString &y) {
        qsizetype i = 0, j = 0;
        while (i < x.size() || j < y.size()) {
            while (i < x.size() && !x[i].isLetterOrNumber()) {
                ++i;
            }
            while (j < y.size() && !y[j].isLetterOrNumber()) {
                ++j;
            }
            if (i >= x.size() || j >= y.size()) {
                break;
            }
            const bool numeric = x[i].isDigit();
            qsizetype ei = i, ej = j;
            while (ei < x.size() && (numeric ? x[ei].isDigit() : x[ei].isLetter())) {
                ++ei;
            }
            while (ej < y.size() && (numeric ? y[ej].isDigit() : y[ej].isLetter())) {
                ++ej;
            }
            if (ej == j) {
                return numeric ? 1 : -1;
            }
            QString a1 = x.mid(i, ei - i), b1 = y.mid(j, ej - j);
            if (numeric) {
                while (a1.size() > 1 && a1.startsWith(QLatin1Char('0'))) {
                    a1.remove(0, 1);
                }
                while (b1.size() > 1 && b1.startsWith(QLatin1Char('0'))) {
                    b1.remove(0, 1);
                }
                if (a1.size() != b1.size()) {
                    return a1.size() < b1.size() ? -1 : 1;
                }
            }
            const int c = a1.compare(b1);
            if (c != 0) {
                return c < 0 ? -1 : 1;
            }
            i = ei;
            j = ej;
        }
        if (i >= x.size() && j >= y.size()) {
            return 0;
        }
        return i >= x.size() ? -1 : 1;
    };
    QString ea, va, ra, eb, vb, rb;
    split(a, ea, va, ra);
    split(b, eb, vb, rb);
    if (int c = rpm(ea, eb)) {
        return c;
    }
    if (int c = rpm(va, vb)) {
        return c;
    }
    if (!ra.isEmpty() && !rb.isEmpty()) {
        return rpm(ra, rb);
    }
    return 0;
}

QList<PlannedPackage> classify(const QList<std::pair<QString, QString>> &printed, const QHash<QString, QString> &installed)
{
    QList<PlannedPackage> result;
    for (const auto &[name, version] : printed) {
        PlannedPackage p{name, version, PlannedPackage::Change::New, installed.value(name)};
        if (installed.contains(name)) {
            const int c = vercmp(version, p.installed);
            p.change = c == 0 ? PlannedPackage::Change::Same : c > 0 ? PlannedPackage::Change::Upgrade : PlannedPackage::Change::Downgrade;
        }
        result.append(p);
    }
    return result;
}

QStringList names(const QList<PlannedPackage> &packages)
{
    QStringList result;
    for (const PlannedPackage &p : packages) {
        result.append(p.name);
    }
    return result;
}

QString changeName(PlannedPackage::Change c)
{
    switch (c) {
    case PlannedPackage::Change::New:
        return QStringLiteral("new");
    case PlannedPackage::Change::Same:
        return QStringLiteral("same");
    case PlannedPackage::Change::Upgrade:
        return QStringLiteral("upgrade");
    case PlannedPackage::Change::Downgrade:
        return QStringLiteral("downgrade");
    }
    return {};
}

QString stepKindName(PlanStep::Kind k)
{
    switch (k) {
    case PlanStep::Kind::Install:
        return QStringLiteral("install");
    case PlanStep::Kind::Reinstall:
        return QStringLiteral("reinstall");
    case PlanStep::Kind::Files:
        return QStringLiteral("files");
    case PlanStep::Kind::Remove:
        return QStringLiteral("remove");
    case PlanStep::Kind::MarkDependencies:
        return QStringLiteral("mark-dependencies");
    case PlanStep::Kind::MarkExplicit:
        return QStringLiteral("mark-explicit");
    }
    return {};
}
}

std::optional<Queue> Queue::fromJson(const QJsonObject &json, QString *error)
{
    auto fail = [error](const QString &why) -> std::optional<Queue> {
        if (error) {
            *error = why;
        }
        return std::nullopt;
    };
    if (json.value(QStringLiteral("version")).toInt() != 1) {
        return fail(QStringLiteral("unknown queue version"));
    }
    Queue queue;
    for (const QJsonValue &v : json.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        QueueItem item;
        item.id = o.value(QStringLiteral("id")).toString();
        const QString kind = o.value(QStringLiteral("kind")).toString();
        if (!kindNames().contains(kind)) {
            return fail(QStringLiteral("unknown item kind: %1").arg(kind));
        }
        item.kind = kindNames().value(kind);
        item.name = o.value(QStringLiteral("name")).toString();
        item.asDependency = o.value(QStringLiteral("asDependency")).toBool();
        item.withSettings = o.value(QStringLiteral("withSettings")).toBool();
        item.explicitReason = o.value(QStringLiteral("explicit")).toBool(true);
        item.file = o.value(QStringLiteral("file")).toString();
        item.fileSize = o.value(QStringLiteral("size")).toInteger(-1);
        item.fileSha256 = o.value(QStringLiteral("sha256")).toString();
        if (item.kind == QueueItem::Kind::File) {
            if (!QDir::isAbsolutePath(item.file) || !item.file.contains(QLatin1String(".pkg.tar"))) {
                return fail(QStringLiteral("not a package file: %1").arg(item.file));
            }
        } else if (!validName(item.name)) {
            return fail(QStringLiteral("not a package name: %1").arg(item.name));
        }
        queue.items.append(item);
    }
    return queue;
}

QJsonObject Queue::toJson() const
{
    QJsonArray items;
    for (const QueueItem &item : this->items) {
        QJsonObject o{{QStringLiteral("id"), item.id}, {QStringLiteral("kind"), kindNames().key(item.kind)}};
        if (item.kind == QueueItem::Kind::File) {
            o.insert(QStringLiteral("file"), item.file);
            o.insert(QStringLiteral("size"), item.fileSize);
            o.insert(QStringLiteral("sha256"), item.fileSha256);
        } else {
            o.insert(QStringLiteral("name"), item.name);
        }
        if (item.asDependency) {
            o.insert(QStringLiteral("asDependency"), true);
        }
        if (item.withSettings) {
            o.insert(QStringLiteral("withSettings"), true);
        }
        if (item.kind == QueueItem::Kind::Reason) {
            o.insert(QStringLiteral("explicit"), item.explicitReason);
        }
        items.append(o);
    }
    return {{QStringLiteral("version"), 1}, {QStringLiteral("items"), items}};
}

bool Plan::runnable() const
{
    if (steps.isEmpty()) {
        return false;
    }
    for (const PlanStep &s : steps) {
        if (!s.blocked.isEmpty()) {
            return false;
        }
        if (offline && (s.kind == PlanStep::Kind::Install || s.kind == PlanStep::Kind::Reinstall || s.kind == PlanStep::Kind::Files)) {
            return false;
        }
    }
    return true;
}

QString Plan::stepTitle(const PlanStep &step)
{
    const QString list = step.targets.size() <= 3 ? step.targets.join(QStringLiteral(", "))
                                                  : I18n::tr("%1 and %2 more").arg(step.targets.mid(0, 2).join(QStringLiteral(", "))).arg(step.targets.size() - 2);
    switch (step.kind) {
    case PlanStep::Kind::Install:
        return I18n::tr("Install %1").arg(list);
    case PlanStep::Kind::Reinstall:
        return I18n::tr("Reinstall %1").arg(list);
    case PlanStep::Kind::Files:
        return I18n::tr("Install %1 from a file").arg(list);
    case PlanStep::Kind::Remove:
        return I18n::tr("Remove %1").arg(list);
    case PlanStep::Kind::MarkDependencies:
        return I18n::tr("Mark %1 as installed as a dependency").arg(list);
    case PlanStep::Kind::MarkExplicit:
        return I18n::tr("Mark %1 as explicitly installed").arg(list);
    }
    return {};
}

QJsonObject Plan::toJson() const
{
    QJsonArray s;
    for (const PlanStep &step : steps) {
        QJsonArray packages;
        for (const PlannedPackage &p : step.packages) {
            packages.append(QJsonObject{{QStringLiteral("name"), p.name},
                                        {QStringLiteral("version"), p.version},
                                        {QStringLiteral("change"), changeName(p.change)},
                                        {QStringLiteral("installed"), p.installed}});
        }
        QJsonObject o{{QStringLiteral("kind"), stepKindName(step.kind)},
                      {QStringLiteral("title"), stepTitle(step)},
                      {QStringLiteral("targets"), QJsonArray::fromStringList(step.targets)},
                      {QStringLiteral("command"), QJsonArray::fromStringList(QStringList{QStringLiteral("pacman")} + step.arguments)},
                      {QStringLiteral("packages"), packages},
                      {QStringLiteral("items"), QJsonArray::fromStringList(step.items)}};
        if (!step.blocked.isEmpty()) {
            o.insert(QStringLiteral("blocked"), step.blocked);
            o.insert(QStringLiteral("blockedBy"), QJsonArray::fromStringList(step.blockedBy));
        }
        s.append(o);
    }
    auto notes = [](const QList<PlanNote> &list) {
        QJsonArray a;
        for (const PlanNote &n : list) {
            a.append(QJsonObject{{QStringLiteral("item"), n.item}, {QStringLiteral("text"), n.text}});
        }
        return a;
    };
    return {{QStringLiteral("version"), 1},
            {QStringLiteral("steps"), s},
            {QStringLiteral("updateFirst"), updateFirst},
            {QStringLiteral("pending"), pending},
            {QStringLiteral("offline"), offline},
            {QStringLiteral("refreshError"), refreshError},
            {QStringLiteral("dropped"), notes(dropped)},
            {QStringLiteral("notes"), notes(this->notes)},
            {QStringLiteral("runnable"), runnable()}};
}

QList<std::pair<QString, QString>> parseNameVersion(const QByteArray &output)
{
    QList<std::pair<QString, QString>> result;
    static const QRegularExpression line(QStringLiteral("^([a-z0-9@_+][a-z0-9@._+-]*) (\\S+)$"));
    for (const QString &l : lines(output)) {
        const auto m = line.match(l);
        if (m.hasMatch()) {
            result.append({m.captured(1), m.captured(2)});
        }
    }
    return result;
}

QStringList dependantsOf(const QByteArray &output)
{
    QStringList result;
    static const QRegularExpression breaks(QStringLiteral("breaks dependency '[^']*' required by (\\S+)"));
    for (const QString &l : lines(output)) {
        const auto m = breaks.match(l);
        if (m.hasMatch() && !result.contains(m.captured(1))) {
            result.append(m.captured(1));
        }
    }
    return result;
}

int countUpdates(const QByteArray &output)
{
    static const QRegularExpression update(QStringLiteral("^\\S+ \\S+ -> \\S+$"));
    int n = 0;
    for (const QString &l : lines(output)) {
        n += update.match(l).hasMatch() ? 1 : 0;
    }
    return n;
}

CommandResult SystemPlanEnvironment::run(const QString &program, const QStringList &arguments, const QMap<QString, QString> &environment)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    for (auto it = environment.cbegin(); it != environment.cend(); ++it) {
        env.insert(it.key(), it.value());
    }
    p.setProcessEnvironment(env);
    p.setStandardInputFile(QProcess::nullDevice());
    p.start(program, arguments);
    CommandResult r;
    if (!p.waitForStarted()) {
        return r;
    }
    r.started = true;
    if (!p.waitForFinished(180000)) {
        p.kill();
        p.waitForFinished(1000);
    }
    r.exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    r.out = p.readAllStandardOutput();
    r.err = p.readAllStandardError();
    return r;
}

bool SystemPlanEnvironment::isFile(const QString &path)
{
    return QFileInfo(path).isFile();
}

Plan makePlan(const Queue &queue, PlanEnvironment &env, const PlanOptions &options)
{
    Plan plan;
    const QString planDb = options.planDb.isEmpty() ? Paths::planDbDir() : options.planDb;
    const QStringList printFormat{QStringLiteral("--print"), QStringLiteral("--print-format"), QStringLiteral("%n %v")};

    // What is installed, and how.
    QHash<QString, QString> installed;
    for (const auto &[name, version] : parseNameVersion(env.run(options.pacman, {QStringLiteral("-Q")}).out)) {
        installed.insert(name, version);
    }
    QSet<QString> explicitly;
    for (const QString &name : lines(env.run(options.pacman, {QStringLiteral("-Qqe")}).out)) {
        explicitly.insert(name);
    }
    auto confList = [&](const QString &key) {
        return lines(env.run(options.pacmanConf, {key}).out);
    };
    const QStringList ignored = confList(QStringLiteral("IgnorePkg"));
    const QStringList hold = confList(QStringLiteral("HoldPkg"));

    QList<const QueueItem *> installs, reinstalls, removes, files, reasons;
    for (const QueueItem &item : queue.items) {
        switch (item.kind) {
        case QueueItem::Kind::Install:
            installs.append(&item);
            break;
        case QueueItem::Kind::Reinstall:
            reinstalls.append(&item);
            break;
        case QueueItem::Kind::Remove:
            removes.append(&item);
            break;
        case QueueItem::Kind::File:
            files.append(&item);
            break;
        case QueueItem::Kind::Reason:
            reasons.append(&item);
            break;
        }
    }

    // The planning copy, fresh (checkupdates syncs it without root, its
    // "local" a link to the system's); what it lists is pending.
    const bool needsLists = !installs.isEmpty() || !reinstalls.isEmpty() || !files.isEmpty();
    if (needsLists && options.refresh) {
        QDir().mkpath(planDb);
        QFile::remove(planDb + QStringLiteral("/db.lck")); // only the planner uses this copy
        const CommandResult r = env.run(options.checkupdates, {QStringLiteral("--nocolor")}, {{QStringLiteral("CHECKUPDATES_DB"), planDb}});
        if (!r.started || (r.exitCode != 0 && r.exitCode != 2)) {
            plan.offline = true;
            plan.refreshError = r.started ? lastError(r) : QStringLiteral("checkupdates not found");
        } else {
            plan.pending = r.exitCode == 0 ? countUpdates(r.out) : 0;
        }
    }
    const QStringList freshDb{QStringLiteral("--dbpath"), planDb};

    // --- One install step: every target, --needed --------------------------
    QStringList installTargets;
    QStringList installIds;
    QStringList newDependencies;
    for (const QueueItem *item : std::as_const(installs)) {
        if (installed.contains(item->name)) {
            // Installed already: nothing to install (and its reason stays).
            plan.dropped.append({item->id, I18n::tr("%1 is already installed.").arg(item->name)});
            continue;
        }
        if (!installTargets.contains(item->name)) {
            installTargets.append(item->name);
        }
        installIds.append(item->id);
        if (item->asDependency) {
            newDependencies.append(item->name);
        }
        if (ignored.contains(item->name)) {
            plan.notes.append({item->id, I18n::tr("%1 is in IgnorePkg (pacman.conf): pacman asks whether to install it anyway.").arg(item->name)});
        }
    }
    if (!installTargets.isEmpty()) {
        PlanStep step;
        step.kind = PlanStep::Kind::Install;
        step.targets = installTargets;
        step.arguments = QStringList{QStringLiteral("-S"), QStringLiteral("--needed")} + installTargets;
        step.items = installIds;
        const CommandResult sys = env.run(options.pacman, QStringList{QStringLiteral("-S")} + printFormat + installTargets);
        if (sys.exitCode != 0) {
            step.blocked = lastError(sys);
        } else {
            const auto printed = parseNameVersion(sys.out);
            step.packages = classify(printed, installed);
            const QStringList got = names(step.packages);
            // A target missing from its own dry run is a group or a virtual name.
            for (const QString &t : std::as_const(installTargets)) {
                if (!got.contains(t)) {
                    step.blockedBy.append(t);
                }
            }
            if (!step.blockedBy.isEmpty()) {
                step.blocked = I18n::tr("Not a package: %1. Choose the package itself (a group's members, or which package provides it).")
                                   .arg(step.blockedBy.join(QStringLiteral(", ")));
            }
            // Update first, unless the system is current and the install
            // brings nothing newer than what is there.
            const CommandResult fresh = env.run(options.pacman, freshDb + QStringList{QStringLiteral("-S")} + printFormat + installTargets);
            const bool sameOnBoth = fresh.exitCode == 0 && parseNameVersion(fresh.out) == printed;
            const bool upgradesInside = std::any_of(step.packages.cbegin(), step.packages.cend(), [](const PlannedPackage &p) {
                return p.change == PlannedPackage::Change::Upgrade || p.change == PlannedPackage::Change::Downgrade;
            });
            if (plan.pending > 0 || !sameOnBoth || upgradesInside) {
                plan.updateFirst = true;
            }
        }
        plan.steps.append(step);
    }

    // --- Reinstall: only when local, system and fresh versions agree ---------
    QStringList reinstallTargets, reinstallIds;
    for (const QueueItem *item : std::as_const(reinstalls)) {
        const QString local = installed.value(item->name);
        auto versionIn = [&](const QStringList &db) {
            const CommandResult r = env.run(options.pacman, db + QStringList{QStringLiteral("-S")} + printFormat + QStringList{item->name});
            for (const auto &[n, v] : parseNameVersion(r.out)) {
                if (n == item->name) {
                    return v;
                }
            }
            return QString();
        };
        const QString sys = versionIn({});
        const QString fresh = plan.offline ? sys : versionIn(freshDb);
        if (local.isEmpty() || sys.isEmpty()) {
            plan.dropped.append({item->id, I18n::tr("%1 cannot be reinstalled: it is not installed from a repository.").arg(item->name)});
        } else if (local != sys || sys != fresh) {
            plan.dropped.append({item->id, I18n::tr("%1 has a newer version: updating installs it.").arg(item->name)});
        } else {
            reinstallTargets.append(item->name);
            reinstallIds.append(item->id);
        }
    }
    if (!reinstallTargets.isEmpty()) {
        PlanStep step;
        step.kind = PlanStep::Kind::Reinstall;
        step.targets = reinstallTargets;
        step.arguments = QStringList{QStringLiteral("-S")} + reinstallTargets;
        step.items = reinstallIds;
        const CommandResult sys = env.run(options.pacman, QStringList{QStringLiteral("-S")} + printFormat + reinstallTargets);
        if (sys.exitCode != 0) {
            step.blocked = lastError(sys);
        } else {
            step.packages = classify(parseNameVersion(sys.out), installed);
        }
        plan.steps.append(step);
    }

    // --- Package files ---------------------------------------------------------
    QStringList fileTargets, fileIds;
    for (const QueueItem *item : std::as_const(files)) {
        if (!env.isFile(item->file)) {
            plan.dropped.append({item->id, I18n::tr("%1 is gone.").arg(item->file)});
            continue;
        }
        fileTargets.append(item->file);
        fileIds.append(item->id);
    }
    if (!fileTargets.isEmpty()) {
        PlanStep step;
        step.kind = PlanStep::Kind::Files;
        step.targets = fileTargets;
        step.arguments = QStringList{QStringLiteral("-U")} + fileTargets;
        step.items = fileIds;
        const CommandResult sys = env.run(options.pacman, QStringList{QStringLiteral("-U")} + printFormat + fileTargets);
        if (sys.exitCode != 0) {
            step.blocked = lastError(sys);
        } else {
            step.packages = classify(parseNameVersion(sys.out), installed);
            // The files' own packages may replace installed ones; anything
            // else upgraded with them is a partial upgrade.
            const qsizetype own = fileTargets.size();
            for (qsizetype i = 0; i < step.packages.size() - own; ++i) {
                const auto change = step.packages[i].change;
                if (change == PlannedPackage::Change::Upgrade || change == PlannedPackage::Change::Downgrade) {
                    plan.updateFirst = true;
                }
            }
            if (plan.pending > 0) {
                plan.updateFirst = true;
            }
        }
        plan.steps.append(step);
    }

    // --- Removals: -Rs, and -Rns for those with their settings ----------------
    QStringList pulledIn;
    for (const PlanStep &s : std::as_const(plan.steps)) {
        pulledIn += names(s.packages);
    }
    for (const bool withSettings : {false, true}) {
        QStringList targets, ids;
        for (const QueueItem *item : std::as_const(removes)) {
            if (item->withSettings != withSettings) {
                continue;
            }
            if (!installed.contains(item->name)) {
                plan.dropped.append({item->id, I18n::tr("%1 is not installed.").arg(item->name)});
                continue;
            }
            targets.append(item->name);
            ids.append(item->id);
        }
        if (targets.isEmpty()) {
            continue;
        }
        PlanStep step;
        step.kind = PlanStep::Kind::Remove;
        step.targets = targets;
        step.arguments = QStringList{withSettings ? QStringLiteral("-Rns") : QStringLiteral("-Rs")} + targets;
        step.items = ids;
        for (const QString &t : std::as_const(targets)) {
            if (hold.contains(t)) {
                step.blockedBy.append(t);
            }
        }
        const CommandResult sys = env.run(options.pacman, QStringList{QStringLiteral("-Rs")} + printFormat + targets);
        if (!step.blockedBy.isEmpty()) {
            step.blocked = I18n::tr("pacman keeps %1 (HoldPkg): the system needs it.").arg(step.blockedBy.join(QStringLiteral(", ")));
        } else if (sys.exitCode != 0) {
            step.blockedBy = dependantsOf(sys.out + sys.err);
            step.blocked = step.blockedBy.isEmpty() ? lastError(sys)
                                                    : I18n::tr("Other installed packages need it: %1.").arg(step.blockedBy.join(QStringLiteral(", ")));
        } else {
            step.packages = classify(parseNameVersion(sys.out), installed);
            for (const QString &n : names(step.packages)) {
                if (pulledIn.contains(n)) {
                    step.blockedBy.append(n);
                }
            }
            if (!step.blockedBy.isEmpty()) {
                step.blocked = I18n::tr("The same packages are to be installed and removed: %1.").arg(step.blockedBy.join(QStringLiteral(", ")));
            }
        }
        plan.steps.append(step);
    }

    // --- Install reasons --------------------------------------------------------
    QStringList toDependency = newDependencies, toExplicit, depIds, explicitIds;
    for (const QueueItem *item : std::as_const(installs)) {
        if (newDependencies.contains(item->name)) {
            depIds.append(item->id);
        }
    }
    for (const QueueItem *item : std::as_const(reasons)) {
        if (!installed.contains(item->name)) {
            plan.dropped.append({item->id, I18n::tr("%1 is not installed.").arg(item->name)});
        } else if (item->explicitReason == explicitly.contains(item->name)) {
            plan.dropped.append({item->id, I18n::tr("%1 is marked so already.").arg(item->name)});
        } else if (item->explicitReason) {
            toExplicit.append(item->name);
            explicitIds.append(item->id);
        } else {
            toDependency.append(item->name);
            depIds.append(item->id);
        }
    }
    if (!toDependency.isEmpty()) {
        plan.steps.append({PlanStep::Kind::MarkDependencies, toDependency, QStringList{QStringLiteral("-D"), QStringLiteral("--asdeps")} + toDependency, {}, depIds, {}, {}});
    }
    if (!toExplicit.isEmpty()) {
        plan.steps.append({PlanStep::Kind::MarkExplicit, toExplicit, QStringList{QStringLiteral("-D"), QStringLiteral("--asexplicit")} + toExplicit, {}, explicitIds, {}, {}});
    }
    return plan;
}
}
