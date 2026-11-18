#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

// Installing and removing: what the person asked for (the queue), and what
// pacman will be asked to do, step by step (the plan). docs/PACKAGE-MANAGER.md,
// "Step 1 in detail". Planned without root, with pacman's own dry runs
// (--print); kyu-update --plan prints a plan, --apply plans again and runs it.
namespace Kyu
{
struct QueueItem {
    enum class Kind { Install, Reinstall, Remove, File, Reason };
    QString id; // the app's, for Pending
    Kind kind = Kind::Install;
    QString name; // a package (Install, Reinstall, Remove, Reason)
    bool asDependency = false; // Install: an optional dependency, a provider
    bool withSettings = false; // Remove: also its saved settings (-n)
    bool explicitReason = true; // Reason: explicitly installed (else: as a dependency)
    QString file; // File: an absolute path
    qint64 fileSize = -1; // File: as when queued, checked before running
    QString fileSha256;
};

struct Queue {
    QList<QueueItem> items;

    static std::optional<Queue> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

// A package in a dry run, against what is installed.
struct PlannedPackage {
    enum class Change { New, Same, Upgrade, Downgrade };
    QString name;
    QString version;
    Change change = Change::New;
    QString installed; // the installed version, when there is one

    bool operator==(const PlannedPackage &) const = default;
};

struct PlanStep {
    enum class Kind { Install, Reinstall, Files, Remove, MarkDependencies, MarkExplicit };
    Kind kind = Kind::Install;
    QStringList targets; // names, or the files
    QStringList arguments; // pacman's, without sudo: {"-S", "--needed", "gimp"}
    QList<PlannedPackage> packages; // what its dry run printed (none for -D)
    QStringList items; // the queue's ids this step does
    QString blocked; // why it cannot run (a failing dry run), in plain words
    QStringList blockedBy; // what it is about: dependants, missing names

    bool operator==(const PlanStep &) const = default;
};

struct PlanNote {
    QString item; // the queue's id
    QString text; // why the item is not in the plan, or what pacman will ask
};

struct Plan {
    QList<PlanStep> steps;
    bool updateFirst = false; // installing needs the pending updates first
    int pending = 0; // updates in the fresh copy
    bool offline = false; // the package lists could not be refreshed
    QString refreshError;
    QList<PlanNote> dropped; // items not needed or not possible, with why
    QList<PlanNote> notes; // what pacman will ask about (IgnorePkg)

    // Runnable: something to do, nothing blocked, installs not offline.
    bool runnable() const;
    QJsonObject toJson() const;
    // The words for a step: "Install GIMP", "Remove calligra".
    static QString stepTitle(const PlanStep &step);
};

// What the planner runs; the system's, or a test's.
struct CommandResult {
    bool started = false;
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
};
class PlanEnvironment
{
public:
    virtual ~PlanEnvironment() = default;
    // In the C locale, stdin closed, a time limit.
    virtual CommandResult run(const QString &program, const QStringList &arguments, const QMap<QString, QString> &environment = {}) = 0;
    virtual bool isFile(const QString &path) = 0;
};
class SystemPlanEnvironment : public PlanEnvironment
{
public:
    CommandResult run(const QString &program, const QStringList &arguments, const QMap<QString, QString> &environment = {}) override;
    bool isFile(const QString &path) override;
};

struct PlanOptions {
    QString pacman = QStringLiteral("pacman");
    QString pacmanConf = QStringLiteral("pacman-conf");
    QString checkupdates = QStringLiteral("checkupdates");
    QString planDb; // the planning copy (not kyu-check's); empty: Paths::planDbDir()
    bool refresh = true; // refresh the copy first (tests may not)
};

Plan makePlan(const Queue &queue, PlanEnvironment &env, const PlanOptions &options = {});

// Parsing pacman's output (C locale), exposed for tests.
QList<std::pair<QString, QString>> parseNameVersion(const QByteArray &output);
// ":: removing X breaks dependency 'X' required by Y": the Ys.
QStringList dependantsOf(const QByteArray &output);
// checkupdates' lines ("name old -> new").
int countUpdates(const QByteArray &output);
}
