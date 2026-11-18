#pragma once

#include "console.h"
#include "plan.h"

#include <optional>

// kyu-update --plan / --apply: installing and removing what Kyu's Pending
// holds (docs/PACKAGE-MANAGER.md, "Step 1 in detail").
namespace KyuUpdate
{
struct ApplyContext {
    Out *out = nullptr;
    QString elevate; // sudo, doas or pkexec
    QString pacman = QStringLiteral("pacman");
    QString procDir = QStringLiteral("/proc");
    QString dbLock = QStringLiteral("/var/lib/pacman/db.lck");
    QString resultFile; // apply-result.json
    QString expectPath; // the plan the person saw (--expect), compared before anything runs
    Kyu::PlanOptions plan;
};

// The queue file the app wrote; nullopt (and why) when it cannot be read.
std::optional<Kyu::Queue> readQueue(const QString &path, QString *error);

// --plan: the plan as JSON on stdout. 0, or 2 for a queue that cannot be read.
int printPlan(const QString &queuePath, const Kyu::PlanOptions &options);

// Each package file still as it was queued (size and SHA-256).
bool filesUnchanged(const Kyu::Queue &queue, QStringList *changed);

enum class ApplyStart { Run, NeedsUpdate, Nothing, Blocked };
// Plans again, says what will happen; Blocked and Nothing say why.
ApplyStart startApply(ApplyContext &ctx, const Kyu::Queue &queue, Kyu::Plan &plan);

// Runs the plan's steps in Kyu's window, one pacman command each, all in one
// terminal session (so sudo asks for the password once); stops at the first
// answered no or failure. Writes the result file. 0 all done, 1 failed,
// 4 stopped.
int runSteps(ApplyContext &ctx, const Kyu::Queue &queue, const Kyu::Plan &plan);

// After an update that installed the queue's packages in its own
// transaction: the rest of the queue (files, removals, reasons; what was
// meant as a dependency marked so), planned again and run.
int applyRest(ApplyContext &ctx, const Kyu::Queue &queue);

// The install targets an update adds to its own transaction.
QStringList installTargetsFor(const Kyu::Queue &queue, const QString &pacman);
}
