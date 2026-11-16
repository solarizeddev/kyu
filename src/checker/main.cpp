// kyu-check: checks for pending updates and writes them to the state file.
// Runs as the user (from a systemd user timer or the tray); never needs root.

#include "flatpakcheck.h"
#include "news.h"
#include "pacmancheck.h"
#include "paths.h"
#include "updatestate.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QTextStream>

namespace
{
enum ExitCode {
    Ok = 0,
    StateNotSaved = 1,
    SourceFailed = 2,
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kyu-check"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Checks for pending pacman and Flatpak updates and writes them to the Kyu state file."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noPacman(QStringLiteral("no-pacman"), QStringLiteral("Do not check repo packages."));
    QCommandLineOption noFlatpak(QStringLiteral("no-flatpak"), QStringLiteral("Do not check Flatpaks."));
    QCommandLineOption json(QStringLiteral("json"), QStringLiteral("Print the resulting state as JSON."));
    QCommandLineOption stateFile(QStringLiteral("state-file"),
                                 QStringLiteral("State file to update (default: %1).").arg(Kyu::Paths::stateFile()),
                                 QStringLiteral("path"));
    QCommandLineOption checkupdates(QStringLiteral("checkupdates"), QStringLiteral("checkupdates binary."),
                                    QStringLiteral("path"), QStringLiteral("checkupdates"));
    QCommandLineOption pacman(QStringLiteral("pacman"), QStringLiteral("pacman binary."), QStringLiteral("path"),
                              QStringLiteral("pacman"));
    QCommandLineOption noNews(QStringLiteral("no-news"), QStringLiteral("Do not fetch the distro's news."));
    QCommandLineOption newsFeed(QStringLiteral("news-feed"), QStringLiteral("News feed URL."), QStringLiteral("url"));
    QCommandLineOption osRelease(QStringLiteral("os-release"), QStringLiteral("os-release file."), QStringLiteral("path"),
                                 QStringLiteral("/etc/os-release"));
    QCommandLineOption pacmanLog(QStringLiteral("pacman-log"), QStringLiteral("pacman log."), QStringLiteral("path"),
                                 QStringLiteral("/var/log/pacman.log"));
    QCommandLineOption modulesDir(QStringLiteral("modules-dir"), QStringLiteral("Kernel modules directory."), QStringLiteral("path"),
                                  QStringLiteral("/usr/lib/modules"));
    QCommandLineOption kernelRelease(QStringLiteral("kernel-release"), QStringLiteral("Running kernel release."), QStringLiteral("release"));
    for (auto *o : {&newsFeed, &osRelease, &pacmanLog, &modulesDir, &kernelRelease}) {
        o->setFlags(QCommandLineOption::HiddenFromHelp);
    }
    QCommandLineOption dbPath(QStringLiteral("db-path"),
                              QStringLiteral("Database copy for checkupdates (default: %1).").arg(Kyu::Paths::checkupDbDir()),
                              QStringLiteral("path"));
    checkupdates.setFlags(QCommandLineOption::HiddenFromHelp);
    pacman.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions({noPacman, noFlatpak, json, stateFile, checkupdates, pacman, dbPath, noNews, newsFeed, osRelease, pacmanLog,
                       modulesDir, kernelRelease});
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);

    const QString statePath = parser.isSet(stateFile) ? parser.value(stateFile) : Kyu::Paths::stateFile();
    const QString stateDir = QFileInfo(statePath).absolutePath();

    Kyu::UpdateState previous;
    if (QFileInfo::exists(statePath)) {
        QString loadError;
        if (auto loaded = Kyu::UpdateState::load(statePath, &loadError)) {
            previous = *loaded;
        } else {
            err << "kyu-check: ignoring unreadable " << statePath << ": " << loadError << Qt::endl;
        }
    }

    QDir().mkpath(stateDir);
    QLockFile lock(stateDir + QStringLiteral("/check.lock"));
    // A check can take minutes; only treat the lock as stale if its owner died.
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        QString host, appName;
        lock.getLockInfo(&pid, &host, &appName);
        err << "kyu-check: a check is already running (pid " << pid << ")" << Qt::endl;
        return Ok;
    }

    std::optional<Kyu::PacmanResult> pacmanResult;
    if (!parser.isSet(noPacman)) {
        Kyu::PacmanCheckOptions options;
        options.checkupdates = parser.value(checkupdates);
        options.pacman = parser.value(pacman);
        options.dbPath = parser.value(dbPath);
        pacmanResult = Kyu::checkPacman(options);
        if (!pacmanResult->ok) {
            err << "kyu-check: pacman: " << pacmanResult->error << Qt::endl;
        }
        const QString kernel = Kyu::runningKernelPackage(parser.value(modulesDir), parser.value(kernelRelease));
        for (auto &p : pacmanResult->packages) {
            p.restart = !kernel.isEmpty() && p.name == kernel;
        }
    }

    std::optional<Kyu::FlatpakResult> flatpakResult;
    if (!parser.isSet(noFlatpak)) {
        flatpakResult = Kyu::checkFlatpak();
        if (flatpakResult->available && !flatpakResult->ok) {
            err << "kyu-check: flatpak: " << flatpakResult->error << Qt::endl;
        }
    }

    std::optional<Kyu::NewsResult> newsResult;
    if (!parser.isSet(noNews)) {
        Kyu::NewsSource source = Kyu::newsSourceFor(parser.value(osRelease));
        if (parser.isSet(newsFeed)) {
            source.url = parser.value(newsFeed);
            if (source.name.isEmpty()) {
                source.name = QStringLiteral("News");
            }
        }
        newsResult = Kyu::fetchNews(source, Kyu::lastFullUpgrade(parser.value(pacmanLog)));
        if (!newsResult->ok) {
            err << "kyu-check: news: " << newsResult->error << Qt::endl;
        }
    }

    const auto state = Kyu::UpdateState::merge(previous, pacmanResult, flatpakResult, QDateTime::currentDateTimeUtc(), newsResult);

    QString saveError;
    if (!state.save(statePath, &saveError)) {
        err << "kyu-check: cannot write " << statePath << ": " << saveError << Qt::endl;
        return StateNotSaved;
    }

    if (parser.isSet(json)) {
        out << QJsonDocument(state.toJson()).toJson(QJsonDocument::Indented);
    } else {
        out << state.summary() << Qt::endl;
    }
    return state.hasError() ? SourceFailed : Ok;
}
