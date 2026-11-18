#include "apply.h"

#include "failure.h"
#include "failuretext.h"
#include "i18n.h"
#include "translations.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>

namespace KyuUpdate
{
namespace
{
// The result file: per queue item done, stopped, failed, not run or
// dropped; the app derives Pending from the local database and uses this
// only to say why.
class Result
{
public:
    Result(const QString &path, const Kyu::Queue &queue)
        : m_path(path)
    {
        for (const Kyu::QueueItem &item : queue.items) {
            m_items.insert(item.id, QStringLiteral("notRun"));
        }
        m_time = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    void set(const QStringList &ids, const QString &status)
    {
        for (const QString &id : ids) {
            m_items.insert(id, status);
        }
        save();
    }
    void save() const
    {
        if (m_path.isEmpty()) {
            return;
        }
        QJsonObject items;
        for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
            items.insert(it.key(), it.value());
        }
        QSaveFile f(m_path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(QJsonObject{{QStringLiteral("time"), m_time}, {QStringLiteral("items"), items}}).toJson());
            f.commit();
        }
    }

private:
    QString m_path;
    QString m_time;
    QMap<QString, QString> m_items;
};

// pacman's question answered no ("[Y/n] n", "[J/n] n"), or Ctrl+C.
bool answeredNo(const QStringList &shown, int code)
{
    static const QRegularExpression answered(QStringLiteral("\\[[^\\[\\]/\\s]{1,4}/[^\\[\\]/\\s]{1,4}\\]\\s*\\S*\\s*$"));
    return code == 130 || (!shown.isEmpty() && shown.last().startsWith(QLatin1String("::")) && answered.match(shown.last()).hasMatch());
}

QString sha256(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return QString::fromLatin1(hash.result().toHex());
}

void sayPlan(Out &out, const Kyu::Plan &plan)
{
    for (const Kyu::PlanNote &n : plan.dropped) {
        out.note(n.text, Tone::Dim);
    }
    for (const Kyu::PlanNote &n : plan.notes) {
        out.note(n.text);
    }
}

// Waits while another package manager holds pacman's lock (pamac's
// background refresh, most often); false when stopped with Ctrl+C.
bool waitForPacman(ApplyContext &ctx)
{
    if (!QFileInfo::exists(ctx.dbLock)) {
        return true;
    }
    const QList<Manager> holders = lockHolders(ctx.procDir, ctx.dbLock);
    ctx.out->warn(holders.isEmpty() ? Kyu::I18n::tr("Waiting for pacman's lock to go (%1).").arg(ctx.dbLock)
                                    : Kyu::I18n::tr("Waiting for %1 to finish. Press Ctrl+C to cancel.").arg(describe(holders.constFirst())));
    interrupted = 0;
    struct sigaction stop {}, old {};
    stop.sa_handler = onInterrupt;
    ::sigaction(SIGINT, &stop, &old);
    while (QFileInfo::exists(ctx.dbLock) && !interrupted && !hungUp) {
        QThread::msleep(500);
    }
    ::sigaction(SIGINT, &old, nullptr);
    return !interrupted && !hungUp;
}
}

std::optional<Kyu::Queue> readQueue(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = f.errorString();
        }
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (!doc.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return std::nullopt;
    }
    return Kyu::Queue::fromJson(doc.object(), error);
}

int printPlan(const QString &queuePath, const Kyu::PlanOptions &options)
{
    QString error;
    const auto queue = readQueue(queuePath, &error);
    if (!queue) {
        std::fprintf(stderr, "kyu-update: %s: %s\n", qPrintable(queuePath), qPrintable(error));
        return 2;
    }
    Kyu::SystemPlanEnvironment env;
    const Kyu::Plan plan = Kyu::makePlan(*queue, env, options);
    std::fputs(QJsonDocument(plan.toJson()).toJson().constData(), stdout);
    return 0;
}

bool filesUnchanged(const Kyu::Queue &queue, QStringList *changed)
{
    for (const Kyu::QueueItem &item : queue.items) {
        if (item.kind != Kyu::QueueItem::Kind::File) {
            continue;
        }
        const QFileInfo info(item.file);
        if ((item.fileSize >= 0 && info.size() != item.fileSize) || (!item.fileSha256.isEmpty() && sha256(item.file) != item.fileSha256)) {
            changed->append(item.file);
        }
    }
    return changed->isEmpty();
}

ApplyStart startApply(ApplyContext &ctx, const Kyu::Queue &queue, Kyu::Plan &plan)
{
    Out &out = *ctx.out;
    QStringList changed;
    if (!filesUnchanged(queue, &changed)) {
        out.bad(Kyu::I18n::tr("A package file changed since it was added: %1. Add it again if you still want it.").arg(changed.join(QStringLiteral(", "))));
        return ApplyStart::Blocked;
    }
    out.quietStep(Kyu::I18n::tr("Planning"), Kyu::I18n::tr("refreshing the package lists (no root)"));
    Kyu::SystemPlanEnvironment env;
    plan = Kyu::makePlan(queue, env, ctx.plan);
    sayPlan(out, plan);
    if (plan.offline) {
        out.warn(Kyu::I18n::tr("The package lists could not be refreshed (%1).").arg(plan.refreshError));
    }
    bool blocked = false;
    for (const Kyu::PlanStep &step : std::as_const(plan.steps)) {
        if (!step.blocked.isEmpty()) {
            out.bad(Kyu::Plan::stepTitle(step) + QStringLiteral(": ") + step.blocked);
            blocked = true;
        }
    }
    if (blocked) {
        return ApplyStart::Blocked;
    }
    if (plan.steps.isEmpty()) {
        out.good(Kyu::I18n::tr("Nothing to do."));
        return ApplyStart::Nothing;
    }
    if (!plan.runnable()) {
        out.bad(Kyu::I18n::tr("Installing needs the network. Removing can go ahead without it."));
        return ApplyStart::Blocked;
    }
    // What pacman will do now, against the plan the person saw: said, and
    // asked, before anything runs.
    if (!ctx.expectPath.isEmpty()) {
        QFile f(ctx.expectPath);
        const QJsonObject expected = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
        QSet<QString> before, now;
        for (const QJsonValue &step : expected.value(QStringLiteral("steps")).toArray()) {
            for (const QJsonValue &p : step.toObject().value(QStringLiteral("packages")).toArray()) {
                before.insert(p.toObject().value(QStringLiteral("name")).toString());
            }
        }
        for (const Kyu::PlanStep &step : std::as_const(plan.steps)) {
            for (const Kyu::PlannedPackage &p : step.packages) {
                now.insert(p.name);
            }
        }
        QStringList added = QStringList(now.cbegin(), now.cend()), gone = QStringList(before.cbegin(), before.cend());
        added.removeIf([&before](const QString &n) {
            return before.contains(n);
        });
        gone.removeIf([&now](const QString &n) {
            return now.contains(n);
        });
        added.sort();
        gone.sort();
        if (!added.isEmpty() || !gone.isEmpty()) {
            out.warn(Kyu::I18n::tr("pacman's list differs from the plan (the package lists changed):"));
            if (!added.isEmpty()) {
                out.note(Kyu::I18n::tr("also: %1").arg(added.join(QStringLiteral(", "))));
            }
            if (!gone.isEmpty()) {
                out.note(Kyu::I18n::tr("no longer: %1").arg(gone.join(QStringLiteral(", "))));
            }
            discardTypedAhead();
            out.prompt(Kyu::I18n::tr("Continue with pacman's list? [y/N] "));
            if (!isYes(readAnswer())) {
                out.warn(Kyu::I18n::tr("Nothing was changed."));
                return ApplyStart::Blocked;
            }
        }
    }
    if (plan.updateFirst) {
        return ApplyStart::NeedsUpdate;
    }
    return ApplyStart::Run;
}

int runSteps(ApplyContext &ctx, const Kyu::Queue &queue, const Kyu::Plan &plan)
{
    Out &out = *ctx.out;
    Result result(ctx.resultFile, queue);
    for (const Kyu::PlanNote &n : plan.dropped) {
        result.set({n.item}, QStringLiteral("dropped"));
    }
    const qsizetype total = plan.steps.size();
    out.windowTitle(total == 1 ? Kyu::Plan::stepTitle(plan.steps.constFirst()) : Kyu::I18n::tr("Installing and removing"));
    if (!waitForPacman(ctx)) {
        out.warn(Kyu::I18n::tr("Stopped. Nothing was changed."));
        return 4;
    }
    // All steps in one terminal session, chained: sudo asks for the password
    // once, and a step answered no or failing ends the session.
    QList<QStringList> commands;
    QStringList headers, shownCommands;
    for (qsizetype i = 0; i < total; ++i) {
        const Kyu::PlanStep &step = plan.steps[i];
        const QString title = Kyu::Plan::stepTitle(step);
        const QString shown = ctx.elevate + QStringLiteral(" pacman ") + shellJoin(step.arguments);
        shownCommands.append(shown);
        commands.append(QStringList{ctx.elevate, ctx.pacman} + step.arguments);
        headers.append(out.headingText(total > 1 ? Kyu::I18n::tr("Step %1 of %2: %3").arg(i + 1).arg(total).arg(title) : title)
                       + out.stepText(Kyu::I18n::tr("pacman"), shown));
    }
    QStringList lines;
    const QList<int> codes = runSession(commands, headers, true, &lines);

    const Kyu::PacmanTranslations translations = Kyu::PacmanTranslations::forEnvironment();
    for (qsizetype i = 0; i < total; ++i) {
        const Kyu::PlanStep &step = plan.steps[i];
        const int code = codes.value(i, -1);
        const bool known = code >= 0;
        if (code == 0) {
            result.set(step.items, QStringLiteral("done"));
            continue;
        }
        // This step's own lines: after its step line.
        QStringList shown = lines;
        for (qsizetype n = lines.size() - 1; n >= 0; --n) {
            if (lines.at(n).endsWith(shownCommands.at(i))) {
                shown = lines.mid(n + 1);
                break;
            }
        }
        const QString title = Kyu::Plan::stepTitle(step);
        const bool no = !known || answeredNo(shown, code) || hungUp;
        out.blank();
        if (no) {
            result.set(step.items, QStringLiteral("stopped"));
            out.warn(Kyu::I18n::tr("Stopped: %1 was not done.").arg(title));
        } else {
            result.set(step.items, QStringLiteral("failed"));
            const QJsonObject failure = Kyu::classifyFailure(translations.toEnglish(shown));
            if (!failure.isEmpty()) {
                const FailureText text = describeFailure(failure);
                out.bad(text.title);
                for (const QString &line : wrap(text.text)) {
                    out.note(line);
                }
                if (!text.command.isEmpty()) {
                    out.fix(text.command);
                }
            } else {
                out.bad(Kyu::I18n::tr("%1 failed (status %2); pacman's messages are above.").arg(title).arg(code));
            }
        }
        if (i + 1 < total) {
            QStringList later;
            for (qsizetype j = i + 1; j < total; ++j) {
                later.append(Kyu::Plan::stepTitle(plan.steps[j]));
            }
            out.note(Kyu::I18n::tr("Not done, still in Pending: %1").arg(later.join(QStringLiteral("; "))), Tone::Dim);
        }
        return no ? 4 : 1;
    }
    out.blank();
    out.good(Kyu::I18n::tr("Done."));
    return 0;
}

QStringList installTargetsFor(const Kyu::Queue &queue, const QString &pacman)
{
    Kyu::SystemPlanEnvironment env;
    QSet<QString> installed;
    for (const auto &[n, v] : Kyu::parseNameVersion(env.run(pacman, {QStringLiteral("-Q")}).out)) {
        installed.insert(n);
    }
    QStringList targets;
    for (const Kyu::QueueItem &item : queue.items) {
        if (item.kind == Kyu::QueueItem::Kind::Install && !installed.contains(item.name) && !targets.contains(item.name)) {
            targets.append(item.name);
        }
    }
    return targets;
}

int applyRest(ApplyContext &ctx, const Kyu::Queue &queue)
{
    // The installs happened in the update's transaction; what was meant as a
    // dependency is now marked so; the rest is planned again on the updated
    // system (its lists are the fresh ones now).
    Kyu::Queue rest;
    for (const Kyu::QueueItem &item : queue.items) {
        if (item.kind == Kyu::QueueItem::Kind::Install) {
            if (item.asDependency) {
                Kyu::QueueItem mark = item;
                mark.kind = Kyu::QueueItem::Kind::Reason;
                mark.explicitReason = false;
                rest.items.append(mark);
            }
            continue;
        }
        rest.items.append(item);
    }
    if (rest.items.isEmpty()) {
        return 0;
    }
    Kyu::PlanOptions options = ctx.plan;
    options.refresh = false;
    Kyu::SystemPlanEnvironment env;
    const Kyu::Plan plan = Kyu::makePlan(rest, env, options);
    sayPlan(*ctx.out, plan);
    for (const Kyu::PlanStep &step : plan.steps) {
        if (!step.blocked.isEmpty()) {
            ctx.out->bad(Kyu::Plan::stepTitle(step) + QStringLiteral(": ") + step.blocked);
            return 1;
        }
    }
    return plan.steps.isEmpty() ? 0 : runSteps(ctx, rest, plan);
}
}
