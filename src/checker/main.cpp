// kyu-check: checks for pending updates and writes them to the state file.
// Runs as the user (from a systemd user timer or the tray); never needs root.

#include "aur.h"
#include "flatpakcheck.h"
#include "i18n.h"
#include "news.h"
#include "pacmancheck.h"
#include "paths.h"
#include "updatestate.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QNetworkProxyFactory>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>

#include <unistd.h>
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
    // A safety net: whatever hangs, the check ends (and its lock with it)
    // after 15 minutes, so it never blocks the checks after it. (Each part has
    // its own, shorter limit; together they stay under this.)
    ::alarm(900);
    QCoreApplication app(argc, argv);
    // Its summary, --help and messages in the user's language.
    Kyu::loadTranslations();
    // The news and the AUR through the system's proxy (http_proxy,
    // https_proxy, no_proxy), as pacman and curl use it; Qt ignores it
    // otherwise.
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QCoreApplication::setApplicationName(QStringLiteral("kyu-check"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        Kyu::I18n::tr("Checks for pending pacman, AUR and Flatpak updates and writes them to the Kyu state file."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noPacman(QStringLiteral("no-pacman"), Kyu::I18n::tr("Do not check repo packages."));
    QCommandLineOption noFlatpak(QStringLiteral("no-flatpak"), Kyu::I18n::tr("Do not check Flatpaks."));
    QCommandLineOption json(QStringLiteral("json"), Kyu::I18n::tr("Print the resulting state as JSON."));
    QCommandLineOption stateFile(QStringLiteral("state-file"),
                                 Kyu::I18n::tr("State file to update (default: %1).").arg(Kyu::Paths::stateFile()),
                                 Kyu::I18n::tr("path"));
    QCommandLineOption checkupdates(QStringLiteral("checkupdates"), Kyu::I18n::tr("checkupdates binary."),
                                    Kyu::I18n::tr("path"), QStringLiteral("checkupdates"));
    QCommandLineOption pacman(QStringLiteral("pacman"), Kyu::I18n::tr("pacman binary."), Kyu::I18n::tr("path"),
                              QStringLiteral("pacman"));
    QCommandLineOption noAur(QStringLiteral("no-aur"), Kyu::I18n::tr("Do not check AUR packages."));
    QCommandLineOption aurHelper(QStringLiteral("aur-helper"), Kyu::I18n::tr("AUR helper (paru, yay, pamac), or none."), Kyu::I18n::tr("program"));
    QCommandLineOption aurRpc(QStringLiteral("aur-rpc"), Kyu::I18n::tr("AUR info URL."), Kyu::I18n::tr("url"));
    QCommandLineOption aurNow(QStringLiteral("aur-now"), Kyu::I18n::tr("Ask the AUR now, even if it was asked in the last hours."));
    QCommandLineOption pacmanConf(QStringLiteral("pacman-conf"), Kyu::I18n::tr("pacman-conf binary."), Kyu::I18n::tr("path"),
                                  QStringLiteral("pacman-conf"));
    QCommandLineOption noNews(QStringLiteral("no-news"), Kyu::I18n::tr("Do not fetch the distro's news."));
    QCommandLineOption newsFeed(QStringLiteral("news-feed"), Kyu::I18n::tr("News feed URL."), Kyu::I18n::tr("url"));
    QCommandLineOption osRelease(QStringLiteral("os-release"), Kyu::I18n::tr("os-release file."), Kyu::I18n::tr("path"),
                                 QStringLiteral("/etc/os-release"));
    QCommandLineOption pacmanLog(QStringLiteral("pacman-log"), Kyu::I18n::tr("pacman log."), Kyu::I18n::tr("path"),
                                 QStringLiteral("/var/log/pacman.log"));
    QCommandLineOption modulesDir(QStringLiteral("modules-dir"), Kyu::I18n::tr("Kernel modules directory."), Kyu::I18n::tr("path"),
                                  QStringLiteral("/usr/lib/modules"));
    QCommandLineOption kernelRelease(QStringLiteral("kernel-release"), Kyu::I18n::tr("Running kernel release."), Kyu::I18n::tr("release"));
    for (auto *o : {&newsFeed, &osRelease, &pacmanLog, &modulesDir, &kernelRelease, &aurHelper, &aurRpc, &pacmanConf}) {
        o->setFlags(QCommandLineOption::HiddenFromHelp);
    }
    QCommandLineOption dbPath(QStringLiteral("db-path"),
                              Kyu::I18n::tr("Database copy for checkupdates (default: %1).").arg(Kyu::Paths::checkupDbDir()),
                              Kyu::I18n::tr("path"));
    checkupdates.setFlags(QCommandLineOption::HiddenFromHelp);
    pacman.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions({noPacman, noFlatpak, noAur, aurNow, aurHelper, aurRpc, pacmanConf, json, stateFile, checkupdates, pacman, dbPath, noNews, newsFeed, osRelease, pacmanLog,
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
            err << "kyu-check: " << Kyu::I18n::tr("ignoring unreadable %1: %2").arg(statePath, loadError) << Qt::endl;
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
        if (pid > 0) {
            err << "kyu-check: " << Kyu::I18n::tr("a check is already running (pid %1)").arg(pid) << Qt::endl;
        } else {
            err << "kyu-check: " << Kyu::I18n::tr("a check is already running") << Qt::endl;
        }
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
                // In English in the state file: newsSourceText() says it.
                source.name = QStringLiteral(QT_TRANSLATE_NOOP("Kyu", "News"));
            }
        }
        // No known news for this distribution: nothing to do.
        if (!source.name.isEmpty()) {
            newsResult = Kyu::fetchNews(source, Kyu::lastFullUpgrade(parser.value(pacmanLog)));
            if (!newsResult->ok) {
                err << "kyu-check: news: " << newsResult->error << Qt::endl;
            }
        }
    }

    Kyu::UpdateState state = Kyu::UpdateState::merge(previous, pacmanResult, flatpakResult, QDateTime::currentDateTimeUtc(), newsResult);
    QString saveError;
    if (!state.save(statePath, &saveError)) {
        err << "kyu-check: " << Kyu::I18n::tr("cannot write %1: %2").arg(statePath, saveError) << Qt::endl;
        return StateNotSaved;
    }

    // AUR packages, only with an AUR helper to update them, and after the
    // rest is saved (the AUR's site can be slow). The AUR gets the names of
    // the packages from outside the repos; it is asked every few hours, and
    // in between its last answer is used, without what was updated since.
    if (!parser.isSet(noAur)) {
        const QString helper = parser.value(aurHelper);
        const auto found = parser.isSet(aurHelper) ? (helper == QLatin1String("none") ? std::nullopt : Kyu::aurHelperFor(helper))
                                                   : Kyu::configuredAurHelper(Kyu::Paths::configFile());
        Kyu::AurResult aurResult;
        if (!found) {
            aurResult.available = false;
        } else {
            QString error;
            const auto installed = Kyu::foreignPackages(parser.value(pacman), &error);
            // The last list, without what is no longer at the listed version.
            QList<Kyu::PackageUpdate> still = state.aurPackages;
            if (installed) {
                still.removeIf([&installed](const Kyu::PackageUpdate &p) {
                    return installed->value(p.name) != p.oldVersion;
                });
                state.aurPackages = still;
            }
            const QDateTime now = QDateTime::currentDateTimeUtc();
            const bool recent = state.aur.ok && state.aur.lastSuccessAt.isValid() && state.aur.lastSuccessAt.secsTo(now) < 6 * 3600
                && state.aur.lastSuccessAt <= now;
            // Asked for by a person ("Check for updates"): the AUR now.
            const QString askedFile = stateDir + QStringLiteral("/aur-now");
            const bool asked = parser.isSet(aurNow) || QFile::exists(askedFile);
            QFile::remove(askedFile);
            if (recent && installed && !asked) {
                aurResult.ok = true;
                aurResult.cached = true;
                aurResult.packages = still;
            } else {
                Kyu::AurCheckOptions options;
                options.pacman = parser.value(pacman);
                options.pacmanConf = parser.value(pacmanConf);
                if (parser.isSet(aurRpc)) {
                    options.rpc = parser.value(aurRpc);
                }
                aurResult = Kyu::checkAur(options);
                if (!aurResult.ok) {
                    err << "kyu-check: AUR: " << aurResult.error << Qt::endl;
                }
            }
        }
        state = Kyu::UpdateState::merge(state, std::nullopt, std::nullopt, state.checkedAt, std::nullopt, aurResult);
        if (!state.save(statePath, &saveError)) {
            err << "kyu-check: " << Kyu::I18n::tr("cannot write %1: %2").arg(statePath, saveError) << Qt::endl;
            return StateNotSaved;
        }
    }

    if (parser.isSet(json)) {
        out << QJsonDocument(state.toJson()).toJson(QJsonDocument::Indented);
    } else {
        out << state.summary() << Qt::endl;
    }
    return state.hasError() ? SourceFailed : Ok;
}
