// kyu-flatpak: updates Flatpaks for the update panel, in the user's session.
//
// Like kyu-helper for pacman, it runs on its own so the panel can close or
// crash without stopping it, and it writes a log the panel follows
// ($XDG_STATE_HOME/kyu/flatpak/<tx>.jsonl, the helper's record format plus
// "source": "flatpak"). With --after it first waits for that pacman
// transaction: Flatpaks update after pacman, and not at all if the user
// cancelled the pacman update or Kyu stopped it at a question.

#include "flatpakupdater.h"
#include "helperclient.h"
#include "jsonlog.h"
#include "paths.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QLockFile>
#include <QProcess>
#include <QTextStream>

#include <unistd.h>

namespace
{
enum ExitCode {
    Success = 0,
    Failed = 1,
    Skipped = 2,
    AlreadyRunning = 3,
};

struct PacmanOutcome {
    bool known = false;
    bool stopped = false; // cancelled, timed out, or stopped at an unknown question
    QString summary;
};

// Follows the helper until transaction `tx` finishes.
PacmanOutcome waitForPacman(const QString &tx, bool sessionBus, Kyu::JsonLog &log)
{
    PacmanOutcome outcome;
    HelperClient helper(sessionBus ? HelperClient::Bus::Session : HelperClient::Bus::System);
    QString error;
    std::optional<QJsonObject> state;
    if (!helper.subscribe(&error) || !(state = helper.state(&error))) {
        log.append(QStringLiteral("output"), {{QStringLiteral("stream"), QStringLiteral("stderr")},
                                              {QStringLiteral("text"), QStringLiteral("cannot reach kyu-helper: %1").arg(error)}});
        return outcome;
    }
    if (state->value(QStringLiteral("transaction")).toObject().value(QStringLiteral("tx")).toString() != tx) {
        log.append(QStringLiteral("output"), {{QStringLiteral("stream"), QStringLiteral("stderr")},
                                              {QStringLiteral("text"), QStringLiteral("pacman transaction %1 is not known to kyu-helper").arg(tx)}});
        return outcome;
    }
    QEventLoop loop;
    QObject::connect(&helper, &HelperClient::record, &loop, [&](const QJsonObject &r) {
        const QString kind = r.value(QStringLiteral("kind")).toString();
        if (kind == QLatin1String("cancelled") || kind == QLatin1String("timeout")
            || (kind == QLatin1String("question") && !r.value(QStringLiteral("handled")).toBool())) {
            outcome.stopped = true;
        } else if (kind == QLatin1String("finished")) {
            outcome.known = true;
            outcome.summary = r.value(QStringLiteral("summary")).toString();
            log.append(QStringLiteral("output"), {{QStringLiteral("stream"), QStringLiteral("stdout")},
                                                  {QStringLiteral("text"), QStringLiteral("pacman finished: %1").arg(outcome.summary)}});
            loop.quit();
        }
    });
    QObject::connect(&helper, &HelperClient::helperLost, &loop, [&] {
        log.append(QStringLiteral("output"), {{QStringLiteral("stream"), QStringLiteral("stderr")},
                                              {QStringLiteral("text"), QStringLiteral("kyu-helper stopped before pacman finished")}});
        loop.quit();
    });
    helper.follow(*state);
    if (!outcome.known) {
        loop.exec();
    }
    return outcome;
}

void runCheck(const QString &self)
{
    const QString check = QFileInfo(self).absolutePath() + QStringLiteral("/kyu-check");
    QProcess::startDetached(QFileInfo(check).isExecutable() ? check : QStringLiteral("kyu-check"), {});
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kyu-flatpak"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Updates all Flatpaks (system and user installations) through libflatpak."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption after(QStringLiteral("after"), QStringLiteral("Wait for this kyu-helper transaction to finish first."), QStringLiteral("tx"));
    QCommandLineOption sessionBus(QStringLiteral("session-bus"), QStringLiteral("kyu-helper is on the session bus (development)."));
    QCommandLineOption noCheck(QStringLiteral("no-check"), QStringLiteral("Do not run kyu-check afterwards."));
    parser.addOptions({after, sessionBus, noCheck});
    parser.process(app);

    QTextStream err(stderr);
    const QString dir = Kyu::Paths::stateDir() + QStringLiteral("/flatpak");
    QDir().mkpath(dir);
    QLockFile lock(Kyu::Paths::stateDir() + QStringLiteral("/flatpak.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        QString host, name;
        lock.getLockInfo(&pid, &host, &name);
        err << "kyu-flatpak: already running (pid " << pid << ")" << Qt::endl;
        return AlreadyRunning;
    }

    Kyu::JsonLog log;
    QString error;
    if (!log.create(dir, QStringLiteral("flatpak"), &error)) {
        err << "kyu-flatpak: cannot create the log in " << dir << ": " << error << Qt::endl;
        return Failed;
    }
    err << "kyu-flatpak: log " << log.path() << Qt::endl;

    log.append(QStringLiteral("started"),
               {{QStringLiteral("pid"), QCoreApplication::applicationPid()},
                {QStringLiteral("uid"), qint64(::getuid())},
                {QStringLiteral("command"), QJsonArray::fromStringList(QCoreApplication::arguments())},
                {QStringLiteral("log"), log.path()},
                {QStringLiteral("after"), parser.value(after)}});

    if (parser.isSet(after)) {
        log.append(QStringLiteral("stage"), {{QStringLiteral("stage"), QStringLiteral("waiting")}});
        const PacmanOutcome pacman = waitForPacman(parser.value(after), parser.isSet(sessionBus), log);
        if (pacman.stopped) {
            log.append(QStringLiteral("finished"),
                       {{QStringLiteral("success"), false},
                        {QStringLiteral("skipped"), true},
                        {QStringLiteral("changed"), 0},
                        {QStringLiteral("summary"), QStringLiteral("Flatpaks not updated: the pacman update was stopped at a question")}});
            return Skipped;
        }
    }

    log.append(QStringLiteral("stage"), {{QStringLiteral("stage"), QStringLiteral("flatpak")}});
    FlatpakUpdater updater(log);
    const FlatpakUpdater::Result result = updater.run();

    QString summary;
    if (result.changed == 0 && result.ok) {
        summary = QStringLiteral("No Flatpak updates");
    } else {
        summary = result.changed == 1 ? QStringLiteral("1 Flatpak updated") : QStringLiteral("%1 Flatpaks updated").arg(result.changed);
        if (!result.ok) {
            summary += QStringLiteral(". Flatpak: %1").arg(result.errors.join(QStringLiteral("; ")));
        }
    }
    log.append(QStringLiteral("finished"),
               {{QStringLiteral("success"), result.ok},
                {QStringLiteral("changed"), result.changed},
                {QStringLiteral("summary"), summary}});
    err << "kyu-flatpak: " << summary << Qt::endl;

    if (!parser.isSet(noCheck)) {
        runCheck(QCoreApplication::applicationFilePath());
    }
    return result.ok ? Success : Failed;
}
