// kyu-update: the update, in your terminal. Runs the real `sudo pacman -Syu`
// (a new keyring first), then `flatpak update`, then the AUR helper's own
// update (paru -Sua, yay -Sua, pamac update --aur), with their own output
// and their own questions
// ("Proceed?" included), and ends with one verdict: done, or why it failed,
// and what is left to do, each with one key: [r] restart, [m] merge the
// configuration files (pacdiff), [f] fix and update again.
//
// Before pacman: the distro news since the last full upgrade (and a
// "Continue? [y/N]" when there is any), and pacman's lock. Nothing here runs
// as root but pacman itself (through sudo, doas or pkexec) and what a key
// runs.

#include "console.h"

#include "aftercare.h"
#include "failure.h"
#include "housekeeping.h"
#include "i18n.h"
#include "failuretext.h"
#include "aur.h"
#include "keyrings.h"
#include "translations.h"
#include "waiting.h"
#include "news.h"
#include "paths.h"
#include "updatestate.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QLocale>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cerrno>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <functional>
#include <optional>
#include <langinfo.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

using namespace KyuUpdate;

// "Can be cleaned up:" and a line per part that qualifies (housekeeping.h).
void printHousekeeping(Out &out, const Kyu::Housekeeping &h)
{
    out.blank();
    out.heading(Kyu::I18n::tr("Can be cleaned up, if you like:"));
    if (!h.orphans.isEmpty()) {
        QString line = Kyu::numbered("%n orphan packages: %1", "1 orphan package: %1", h.orphans.size()).arg(h.orphans.join(QStringLiteral(", ")));
        // pacman -Rs also takes what only they need: said here, not first at its question.
        if (h.orphanRemovals.size() > h.orphans.size()) {
            line += QLatin1Char(' ') + Kyu::numbered("(%n packages in all, with what only they need)", "(1 package in all, with what only they need)", h.orphanRemovals.size());
        }
        out.note(line);
    }
    if (h.cacheWorthCleaning()) {
        out.note(Kyu::I18n::tr("%1 of older and removed packages in the package cache").arg(QLocale().formattedDataSize(h.cacheBytes, 1)));
    }
    if (!h.unusedFlatpaks.isEmpty()) {
        out.note(Kyu::numbered("%n Flatpak runtimes no app uses", "1 Flatpak runtime no app uses", h.unusedFlatpaks.size()));
    }
}

// [c]: each part on its own, each confirmed (pacman's and flatpak's own
// questions; Kyu's for the cache). A part that fails says so; the update's
// verdict stays what it was. What is left afterwards.
Kyu::Housekeeping cleanUp(Out &out, const QString &elevate, const Kyu::Housekeeping &h)
{
    // A part answered no (or that failed) is not offered again in this run:
    // pacman's own question defaults to yes.
    bool orphansKept = false, cacheKept = false, flatpaksKept = false;
    bool stopped = false;
    if (!h.orphans.isEmpty()) {
        out.blank();
        const QString mode = h.orphansRecursive ? QStringLiteral("-Rs") : QStringLiteral("-R");
        out.step(Kyu::I18n::tr("Orphans"), elevate + QStringLiteral(" pacman ") + mode + QLatin1Char(' ') + h.orphans.join(QLatin1Char(' ')));
        //: "n" is the answer to pacman's question: keep it.
        out.note(Kyu::I18n::tr("Installed as dependencies and needed by nothing now. pacman asks first; to keep one, answer n and run:"), Tone::Dim);
        out.note(elevate + QStringLiteral(" pacman -D --asexplicit NAME"), Tone::Dim);
        QStringList shown;
        const int rc = runWatched(QStringList{elevate, QStringLiteral("pacman"), mode} + h.orphans, QProcessEnvironment::systemEnvironment(), &shown);
        if (rc != 0) {
            orphansKept = true;
            // Answered no ("[Y/n] n", in any language) or Ctrl+C: kept, not failed.
            static const QRegularExpression answered(QStringLiteral("\\[[^\\[\\]/\\s]{1,4}/[^\\[\\]/\\s]{1,4}\\]\\s*\\S*\\s*$"));
            const bool no = rc == 130 || (!shown.isEmpty() && shown.last().startsWith(QLatin1String("::")) && answered.match(shown.last()).hasMatch());
            if (no) {
                out.note(Kyu::I18n::tr("Orphans kept."), Tone::Dim);
                stopped = rc == 130;
            } else {
                out.warn(Kyu::I18n::tr("The orphans were not removed (status %1), see above.").arg(rc));
            }
        }
    }
    if (!stopped && h.cacheWorthCleaning()) {
        out.blank();
        out.step(Kyu::I18n::tr("Cache"), Kyu::I18n::tr("%1, then %2")
                                             .arg(QStringLiteral("%1 paccache -rk%2").arg(elevate).arg(Kyu::CacheVersionsKept), QStringLiteral("-ruk0")));
        discardTypedAhead();
        //: Keep [y/N]: Kyu reads "y" as yes.
        out.prompt(Kyu::I18n::tr("Remove %1 from the package cache: older versions (the newest %2 of each stay) and packages no longer installed? [y/N] ")
                       .arg(QLocale().formattedDataSize(h.cacheBytes, 1))
                       .arg(Kyu::CacheVersionsKept));
        const QString answer = readAnswer();
        // Ctrl+C: no, and the rest of the clean-up stops too.
        stopped = answerInterrupted || hungUp;
        if (isYes(answer)) {
            int rc = runWatched({elevate, QStringLiteral("paccache"), QStringLiteral("-rk%1").arg(Kyu::CacheVersionsKept)});
            if (rc == 0) {
                rc = runWatched({elevate, QStringLiteral("paccache"), QStringLiteral("-ruk0")});
            }
            if (rc != 0) {
                cacheKept = true;
                out.warn(Kyu::I18n::tr("The package cache was not cleaned (status %1), see above.").arg(rc));
            }
        } else {
            cacheKept = true;
        }
    }
    if (!stopped && !h.unusedFlatpaks.isEmpty()) {
        out.blank();
        out.step(QStringLiteral("Flatpak"), QStringLiteral("flatpak uninstall --unused"));
        const int rc = runWatched({QStringLiteral("flatpak"), QStringLiteral("uninstall"), QStringLiteral("--unused")});
        if (rc != 0) {
            flatpaksKept = true;
            out.warn(Kyu::I18n::tr("The unused runtimes were not removed (status %1), see above.").arg(rc));
        }
    }
    Kyu::Housekeeping left = Kyu::readHousekeeping();
    if (orphansKept) {
        left.orphans.clear();
    }
    if (cacheKept) {
        left.cacheFiles.clear();
        left.cacheBytes = 0;
    }
    if (flatpaksKept) {
        left.unusedFlatpaks.clear();
    }
    if (!left.any()) {
        if (!orphansKept && !cacheKept && !flatpaksKept && !stopped) {
            out.blank();
            out.good(Kyu::I18n::tr("Cleaned up."));
        }
    } else {
        printHousekeeping(out, left);
    }
    return left;
}

int main(int argc, char **argv)
{
    // The terminal's character set, from the environment: asked before Qt
    // starts, which switches a non-UTF-8 locale to C.UTF-8 for itself.
    std::setlocale(LC_ALL, "");
    // Qt's four lines about a locale that is not UTF-8 (one not generated):
    // Kyu copes (ASCII marks); they would open its screen.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
        if (message.startsWith(QLatin1String("Detected locale"))) {
            return;
        }
        std::fprintf(stderr, "%s%s\n", type == QtWarningMsg ? "warning: " : "", qPrintable(message));
    });
    const bool utf8 = QByteArray(::nl_langinfo(CODESET)) == "UTF-8";
    QCoreApplication app(argc, argv);
    // Kyu's words in the user's language (English under LC_ALL=C.UTF-8);
    // pacman's, sudo's and flatpak's as they print them.
    Kyu::loadTranslations();
    QCoreApplication::setApplicationName(QStringLiteral("kyu-update"));
    {
        struct sigaction hup {};
        hup.sa_handler = onHangup;
        ::sigaction(SIGHUP, &hup, nullptr);
    }
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(Kyu::I18n::tr("Updates the system in this terminal: %1, then %2, then "
                                                   "the AUR helper's update, with the news before and what is left to do after.")
                                         .arg(QStringLiteral("sudo pacman -Syu"), QStringLiteral("flatpak update")));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noWait(QStringLiteral("no-wait"), Kyu::I18n::tr("Do not wait for a key at the end."));
    QCommandLineOption noCheck(QStringLiteral("no-check"), Kyu::I18n::tr("Do not run kyu-check afterwards."));
    QCommandLineOption refresh(QStringLiteral("refresh"), Kyu::I18n::tr("Download all package databases again (pacman -Syyu), as after changing mirrors."));
    QCommandLineOption stateFile(QStringLiteral("state-file"), QStringLiteral("kyu-check's state (testing)."), QStringLiteral("path"));
    QCommandLineOption pacmanLog(QStringLiteral("pacman-log"), QStringLiteral("pacman's log (testing)."), QStringLiteral("path"),
                                 QStringLiteral("/var/log/pacman.log"));
    QCommandLineOption dbLock(QStringLiteral("db-lock"), QStringLiteral("pacman's lock file (testing)."), QStringLiteral("path"),
                              QStringLiteral("/var/lib/pacman/db.lck"));
    QCommandLineOption procDir(QStringLiteral("proc-dir"), QStringLiteral("/proc (testing)."), QStringLiteral("path"), QStringLiteral("/proc"));
    QCommandLineOption modulesDir(QStringLiteral("modules-dir"), QStringLiteral("/usr/lib/modules (testing)."), QStringLiteral("path"),
                                  QStringLiteral("/usr/lib/modules"));
    QCommandLineOption mergeOnly(QStringLiteral("merge"), Kyu::I18n::tr("Only merge the configuration files the last update left (pacdiff)."));
    QCommandLineOption cleanOnly(QStringLiteral("clean"), Kyu::I18n::tr("Only offer to clean up: orphan packages, the package cache, unused Flatpak runtimes."));
    QCommandLineOption lockWait(QStringLiteral("lock-wait"), QStringLiteral("How long to wait for another package manager, in seconds."),
                                QStringLiteral("seconds"), QStringLiteral("600"));
    lockWait.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption idleAfter(QStringLiteral("idle-after"), QStringLiteral("When a waited-for package manager counts as idle, in seconds."),
                                 QStringLiteral("seconds"), QStringLiteral("30"));
    idleAfter.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption newsRead(QStringLiteral("news-read"), QStringLiteral("Do not show the news (they were read in this window)."));
    newsRead.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption synced(QStringLiteral("synced"), QStringLiteral("The package lists were just synced by a fix."));
    synced.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption aurHelperOption(QStringLiteral("aur-helper"), QStringLiteral("AUR helper (paru, yay, pamac), or none."), QStringLiteral("program"));
    aurHelperOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption localeDir(QStringLiteral("locale-dir"), QStringLiteral("Where pacman's translations are (testing)."), QStringLiteral("path"),
                                 QStringLiteral("/usr/share/locale"));
    localeDir.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption hooksDir(QStringLiteral("hooks-dir"), QStringLiteral("pacman's hook directories (testing)."), QStringLiteral("path"));
    parser.addOptions({noWait, noCheck, refresh, mergeOnly, cleanOnly, lockWait, idleAfter, newsRead, synced, aurHelperOption, localeDir, stateFile, pacmanLog, dbLock, procDir, modulesDir, hooksDir});
    parser.process(app);

    Out out(utf8);
    out.windowTitle();
    QDir().mkpath(Kyu::Paths::stateDir());
    QLockFile lock(Kyu::Paths::stateDir() + QStringLiteral("/update.lock"));

    QString elevate;
    for (const char *tool : {"sudo", "doas", "pkexec"}) {
        if (!QStandardPaths::findExecutable(QLatin1String(tool)).isEmpty()) {
            elevate = QLatin1String(tool);
            break;
        }
    }

    // This program's file, for [f]: read now, while it is surely the one
    // running (pacman may replace it).
    const QByteArray self = QFile::encodeName(QCoreApplication::applicationFilePath());

    // What the last line offers, one key each.
    struct Keys {
        bool restart = false;
        QStringList configs; // [m]: the .pacnew/.pacsave files
        std::function<void(const QStringList &)> configsLeft; // after pacdiff, the ones still there
        QString fix; // [f]: a command; after it, the whole update again
        QString fixShown; // how the fix is said, when the command itself is long
        QStringList again; // ... with these options added
        std::function<bool()> fixAllowed; // checked when [f] is pressed
        Kyu::Housekeeping house; // [c]: what can be cleaned up
    };

    // The end: the lock goes first (the tray's card stops saying "Updating"),
    // then the last line offers what is left to do.
    // The exit status says what happened (0 done, 1 pacman failed, 3 already
    // running, 4 stopped at the news, 5 flatpak failed, 6 the AUR helper
    // failed) only without the wait
    // (--no-wait, or no terminal): after it the person has read the result,
    // last-run.json has it, and a non-zero status would only make Konsole
    // post a "Program exited with status" notification.
    const QString activityPath = Kyu::Paths::stateDir() + QStringLiteral("/activity");
    auto finish = [&](int code, const Keys &keys = {}) {
        QFile::remove(activityPath);
        Kyu::setWaiting(QString());
        lock.unlock();
        if (parser.isSet(noWait) || !stdinIsTerminal()) {
            return code;
        }
        Keys k = keys;
        for (;;) {
            QStringList offer;
            // The keys stay as they are in every language: Kyu reads these letters.
            if (!k.fix.isEmpty()) {
                //: Keep the key in brackets as it is: Kyu reads that letter.
                offer.append(Kyu::I18n::tr("[f] fix and try again"));
            }
            if (k.restart) {
                //: Keep the key in brackets as it is: Kyu reads that letter.
                offer.append(Kyu::I18n::tr("[r] restart"));
            }
            if (!k.configs.isEmpty()) {
                //: Keep the key in brackets as it is: Kyu reads that letter.
                offer.append(Kyu::I18n::tr("[m] merge"));
            }
            if (k.house.any()) {
                //: Keep the key in brackets as it is: Kyu reads that letter.
                offer.append(Kyu::I18n::tr("[c] clean up"));
            }
            //: The Enter key
            offer.append(Kyu::I18n::tr("[Enter] close"));
            out.blank();
            // Keys pressed while flatpak or pacdiff ran are not answers to this.
            discardTypedAhead();
            out.prompt(offer.join(QStringLiteral("   ")) + QLatin1Char(' '));
            const QString answer = readAnswer();
            if (answer.isEmpty()) {
                return 0;
            }
            if (answer == QLatin1String("r") && k.restart) {
                // Plasma's own dialog first: programs can save, and it asks.
                // Quietly: without a Plasma session its error is noise.
                QProcess busctl;
                busctl.setStandardOutputFile(QProcess::nullDevice());
                busctl.setStandardErrorFile(QProcess::nullDevice());
                busctl.start(QStringLiteral("busctl"),
                             {QStringLiteral("--user"), QStringLiteral("call"), QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("/LogoutPrompt"),
                              QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("promptReboot")});
                const int prompted = busctl.waitForFinished(10000) && busctl.exitStatus() == QProcess::NormalExit ? busctl.exitCode() : 1;
                if (prompted == 0) {
                    out.note(Kyu::I18n::tr("Plasma asks whether to restart now."), Tone::Dim);
                    continue;
                }
                //: Keep [y/N]: Kyu reads "y" as yes.
                out.prompt(Kyu::I18n::tr("Restart now? Unsaved work in open programs is lost. [y/N] "));
                if (!isYes(readAnswer())) {
                    continue;
                }
                const int rc = runInTerminal(QStringLiteral("systemctl"), {QStringLiteral("reboot")});
                if (rc == 0) {
                    return 0;
                }
                out.bad(Kyu::I18n::tr("systemctl reboot failed (status %1).").arg(rc));
            } else if (answer == QLatin1String("m") && !k.configs.isEmpty()) {
                if (!runPacdiff(out, elevate)) {
                    continue;
                }
                QStringList left;
                for (const QString &path : std::as_const(k.configs)) {
                    if (QFileInfo::exists(path)) {
                        left.append(path);
                    }
                }
                k.configs = left;
                if (k.configsLeft) {
                    k.configsLeft(left);
                }
                if (left.isEmpty()) {
                    out.good(Kyu::I18n::tr("Configuration files merged."));
                }
            } else if (answer == QLatin1String("c") && k.house.any()) {
                k.house = cleanUp(out, elevate, k.house);
            } else if (answer == QLatin1String("f") && !k.fix.isEmpty()) {
                if (k.fixAllowed && !k.fixAllowed()) {
                    continue;
                }
                const int rc = runWatched({QStringLiteral("/bin/sh"), QStringLiteral("-c"), k.fix});
                if (rc != 0) {
                    out.bad(Kyu::I18n::tr("The fix failed (status %1); its messages are above.").arg(rc));
                    continue;
                }
                // The whole update again, as a new run of this program,
                // without asking about the news a second time.
                QStringList args = QCoreApplication::arguments();
                for (const QString &a : std::as_const(k.again) + QStringList{QStringLiteral("--news-read")}) {
                    if (!args.contains(a)) {
                        args.append(a);
                    }
                }
                QList<QByteArray> bytes;
                for (const QString &a : std::as_const(args)) {
                    bytes.append(QFile::encodeName(a));
                }
                QList<char *> argvNew;
                for (QByteArray &b : bytes) {
                    argvNew.append(b.data());
                }
                argvNew.append(nullptr);
                out.blank();
                ::execv(self.constData(), argvNew.data());
                out.bad(Kyu::I18n::tr("Could not start kyu-update again: run it once more yourself."));
            }
        }
    };

    // One update at a time; the tray's card reads this lock too.
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        QString host, appName;
        lock.getLockInfo(&pid, &host, &appName);
        out.warn(pid > 0 ? Kyu::I18n::tr("An update is already running (pid %1).").arg(pid) : Kyu::I18n::tr("An update is already running."));
        if (parser.isSet(noWait) || !stdinIsTerminal()) {
            return 3;
        }
        out.blank();
        out.prompt(Kyu::I18n::tr("[Enter] close") + QLatin1Char(' '));
        readAnswer();
        return 0;
    }

    // What this run does, for the tray and the card while the lock is held
    // (a file left by a killed run is replaced here).
    {
        QSaveFile activity(activityPath);
        if (activity.open(QIODevice::WriteOnly)) {
            activity.write(parser.isSet(mergeOnly) ? "merge\n" : parser.isSet(cleanOnly) ? "clean\n" : "update\n");
            activity.commit();
        }
    }
    // Nor is any question waiting yet (a file left by a killed run).
    Kyu::setWaiting(QString());

    // --- Clean up only (kyu-update --clean) --------------------------------------
    if (parser.isSet(cleanOnly)) {
        out.header(Kyu::I18n::tr("Kyu · clean up"));
        out.blank();
        Keys keys;
        keys.house = Kyu::readHousekeeping();
        if (!keys.house.any()) {
            out.good(Kyu::I18n::tr("Nothing to clean up."));
        } else {
            printHousekeeping(out, keys.house);
        }
        return finish(0, keys);
    }

    // --- Configuration files only (the card's Merge…) ----------------------------
    if (parser.isSet(mergeOnly)) {
        out.header(Kyu::I18n::tr("Kyu · configuration files"));
        out.blank();
        QJsonObject record = readJson(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
        if (runPacdiff(out, elevate)) {
            QStringList left;
            for (const QString &path : strings(record.value(QStringLiteral("configs")))) {
                if (QFileInfo::exists(path)) {
                    left.append(path);
                }
            }
            if (!record.isEmpty()) {
                record.insert(QStringLiteral("configs"), QJsonArray::fromStringList(left));
                QSaveFile last(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
                if (last.open(QIODevice::WriteOnly)) {
                    last.write(QJsonDocument(record).toJson());
                    last.commit();
                }
            }
            out.blank();
            if (left.isEmpty()) {
                out.good(Kyu::I18n::tr("Configuration files merged."));
            } else {
                out.warn(Kyu::numbered("%n configuration files still to merge.", "1 configuration file still to merge.", left.size()));
            }
        }
        return finish(0);
    }

    // --- Before pacman ----------------------------------------------------
    const QString statePath = parser.isSet(stateFile) ? parser.value(stateFile) : Kyu::Paths::stateFile();
    const std::optional<Kyu::UpdateState> state = Kyu::UpdateState::load(statePath);
    // AUR packages whose build failed last time, at the version still
    // listed: held back until the AUR has a newer one (as the card says),
    // not built again and again.
    QList<Kyu::PackageUpdate> aurPending = state ? state->aurPackages : QList<Kyu::PackageUpdate>();
    QStringList aurHeldBack;
    for (const QJsonValue &v : readJson(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json")).value(QStringLiteral("aurFailed")).toArray()) {
        const QString name = v[QStringLiteral("name")].toString(), version = v[QStringLiteral("version")].toString();
        const auto it = std::find_if(aurPending.begin(), aurPending.end(), [&](const Kyu::PackageUpdate &p) {
            return p.name == name && p.newVersion == version;
        });
        if (it != aurPending.end()) {
            aurPending.erase(it);
            aurHeldBack.append(name);
        }
    }
    const int pendingTotal = state ? int(state->total() - aurHeldBack.size()) : 0;
    if (!state || !state->checkedAt.isValid() || pendingTotal == 0) {
        out.header(Kyu::I18n::tr("Kyu · updating"));
    } else {
        QStringList parts;
        if (!state->packages.isEmpty()) {
            parts.append(Kyu::numbered("%n packages", "1 package", state->packages.size()));
        }
        if (!aurPending.isEmpty()) {
            parts.append(Kyu::I18n::tr("%1 AUR").arg(aurPending.size()));
        }
        if (!state->flatpaks.isEmpty()) {
            parts.append(Kyu::numbered("%n Flatpaks", "1 Flatpak", state->flatpaks.size()));
        }
        out.header(Kyu::numbered("Kyu · %n updates: %1", "Kyu · 1 update: %1", pendingTotal).arg(parts.join(QStringLiteral(", "))));
    }
    if (!aurHeldBack.isEmpty()) {
        out.note(Kyu::I18n::tr("AUR, held back: %1 (not built last time; tried again when the AUR has a newer version).")
                     .arg(aurHeldBack.join(QStringLiteral(", "))),
                 Tone::Dim);
    }
    // Kyu's own window asks before it closes during an update.
    if (qEnvironmentVariableIsEmpty("KYU_WINDOW")) {
        out.note(Kyu::I18n::tr("Keep this window open until Kyu says it is done."), Tone::Dim);
    }

    // The news since the last full upgrade. Asked about once per announcement
    // (seen: confirmed here before, or opened from the card); seen ones are
    // only listed.
    if (state && !state->news.items.isEmpty() && !parser.isSet(newsRead)) {
        const QStringList seen = Kyu::seenNews();
        QList<Kyu::NewsItem> fresh;
        for (const Kyu::NewsItem &item : state->news.items) {
            if (!seen.contains(Kyu::newsKey(item))) {
                fresh.append(item);
            }
        }
        out.blank();
        if (fresh.isEmpty()) {
            // Read before: one quiet line, the link to go back to it.
            const Kyu::NewsItem &newest = state->news.items.constFirst();
            const qsizetype more = state->news.items.size() - 1;
            out.note(Kyu::I18n::tr("News (read): %1").arg(newest.title) + QStringLiteral(" · ") + newest.link
                         + (more > 0 ? QStringLiteral(" · ") + Kyu::numbered("+%n more", "+1 more", more) : QString()),
                     Tone::Dim);
        } else {
            const QList<Kyu::NewsItem> shown = fresh.mid(0, 5);
            out.heading(Kyu::I18n::tr("%1 since your last update").arg(Kyu::newsSourceText(state->news.source)));
            for (const Kyu::NewsItem &item : shown) {
                out.note(item.title);
                out.note(item.link, Tone::Dim);
            }
            if (fresh.size() > shown.size()) {
                out.note(Kyu::numbered("… and %n more: %1", "… and 1 more: %1", fresh.size() - shown.size()).arg(state->news.page), Tone::Dim);
            }
            // No by default: Enter alone must not skip news that may need action.
            //: Keep [y/N]: Kyu reads "y" as yes.
            out.prompt(Kyu::I18n::tr("Continue? [y/N] "));
            //: Keep [y/N]: Kyu reads "y" as yes.
            Kyu::setWaiting(Kyu::I18n::tr("News to read first: continue? [y/N]"));
            const QString answer = readAnswer();
            Kyu::setWaiting(QString());
            if (!stdinIsTerminal()) {
                out.blank(); // a typed answer ends the line itself
            }
            if (!isYes(answer)) {
                out.blank();
                out.warn(Kyu::I18n::tr("Stopped. No packages were upgraded."));
                return finish(4);
            }
            // Read once the person said y, before pacman runs: they saw them,
            // and the next run still lists them. Only the ones shown.
            QStringList keys;
            for (const Kyu::NewsItem &item : shown) {
                keys.append(Kyu::newsKey(item));
            }
            Kyu::markNewsSeen(keys);
        }
    }

    // The AUR helper, if one is installed: last, on its own; its failure is
    // not the system update's.
    const QString aurHelperName = parser.value(aurHelperOption);
    const std::optional<Kyu::AurHelper> aurHelper = parser.isSet(aurHelperOption)
        ? (aurHelperName == QLatin1String("none") ? std::nullopt : Kyu::aurHelperFor(aurHelperName))
        : Kyu::configuredAurHelper(Kyu::Paths::configFile());
    const bool aurWanted = aurHelper && (!state || !state->aur.checked || !state->aur.ok || !aurPending.isEmpty());
    // Only Flatpaks or AUR packages pending (by the last check): no pacman.
    // Not with pamac: its AUR update upgrades the repo packages too, which
    // pacman does here first (news, keyrings, explained failures).
    const bool skipPacman = state && state->pacman.checked && state->pacman.ok && state->packages.isEmpty()
        && (!state->flatpaks.isEmpty() || !aurPending.isEmpty()) && !(aurWanted && aurHelper->name == QLatin1String("pamac"));
    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    const bool flatpakWanted = !flatpak.isEmpty() && (!state || !state->flatpak.checked || !state->flatpak.ok || !state->flatpaks.isEmpty());

    // pacman's lock (not needed for Flatpaks only). Another package manager
    // working (pamac often is, in the background) is waited for; a lock
    // nobody holds, or one whose holder has been idle a while, is left over
    // from a run that was stopped.
    const QString lockPath = parser.value(dbLock), procPath = parser.value(procDir);
    enum class Lock { Free, Stale, Idle, Unseen, Stopped, TimedOut };
    const qint64 lockLimit = parser.value(lockWait).toLongLong() * 1000;
    const qint64 idleLimit = parser.value(idleAfter).toLongLong() * 1000;
    QString idleHolder;
    auto waitForLock = [&]() -> Lock {
        if (!QFileInfo::exists(lockPath)) {
            return Lock::Free;
        }
        // Nobody that could hold it: left over. Managers that all look younger
        // than the lock may be a clock step (NTP, a dual-boot clock): those
        // are waited for like holders, and only ever called idle.
        if (packageManagers(procPath).isEmpty()) {
            return canSeeAllProcesses(procPath) ? Lock::Stale : Lock::Unseen;
        }
        auto candidates = [&]() {
            const QList<Manager> holders = lockHolders(procPath, lockPath);
            return holders.isEmpty() ? packageManagers(procPath) : holders;
        };
        out.warn(Kyu::I18n::tr("Waiting for %1 to finish. Press Ctrl+C to cancel.").arg(describe(candidates().constFirst())));
        // What changes while a package manager works: its and its children's
        // CPU time, pacman's log, the lock itself.
        auto activity = [&]() {
            const QHash<int, int> parents = parentMap(procPath);
            QStringList marks = cpuMarks(procPath, packageManagers(procPath), parents);
            marks.append(QString::number(QFileInfo(parser.value(pacmanLog)).size()));
            marks.append(QString::number(QFileInfo(lockPath).lastModified().toMSecsSinceEpoch()));
            return marks;
        };
        struct sigaction flag {}, old {};
        flag.sa_handler = onInterrupt;
        interrupted = 0;
        ::sigaction(SIGINT, &flag, &old);
        QElapsedTimer waited, quiet;
        waited.start();
        quiet.start();
        QStringList last = activity();
        int freeFor = 0;
        int goneFor = 0;
        Lock result = Lock::TimedOut;
        const bool tty = ::isatty(STDOUT_FILENO);
        while (waited.elapsed() < lockLimit) {
            if (interrupted || hungUp) {
                result = Lock::Stopped;
                break;
            }
            // Free for a moment (three looks): pamac often refreshes, lets
            // go, and takes the lock again at once to upgrade.
            if (!QFileInfo::exists(lockPath)) {
                if (++freeFor >= 3) {
                    result = Lock::Free;
                    break;
                }
            } else {
                freeFor = 0;
                const QList<Manager> all = packageManagers(procPath);
                // Its holder gone: the lock goes a moment later (pacman
                // removes it as it exits). Left over only after 3 s.
                if (all.isEmpty()) {
                    if (++goneFor >= 6) {
                        result = Lock::Stale;
                        break;
                    }
                    QThread::msleep(500);
                    continue;
                }
                goneFor = 0;
                const QList<Manager> holders = candidates();
                const QHash<int, int> parents = parentMap(procPath);
                const bool mayBeIdle = std::all_of(holders.cbegin(), holders.cend(), [&](const Manager &m) {
                    return canBeIdle(m, all, parents);
                });
                if (const QStringList now = activity(); now != last || !mayBeIdle) {
                    last = now;
                    quiet.restart();
                } else if (quiet.elapsed() >= idleLimit) {
                    idleHolder = holders.constFirst().name;
                    result = Lock::Idle;
                    break;
                }
            }
            if (tty) {
                const qint64 s = waited.elapsed() / 1000;
                const QString waitedFor = QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
                std::fputs((QStringLiteral("\r  ") + Kyu::I18n::tr("waited %1").arg(waitedFor) + QLatin1Char(' ')).toLocal8Bit().constData(), stdout);
                std::fflush(stdout);
            }
            QThread::msleep(500);
        }
        ::sigaction(SIGINT, &old, nullptr);
        if (tty) {
            std::printf("\r\x1b[K");
            std::fflush(stdout);
        }
        // Keys pressed while waiting are not answers to pacman.
        discardTypedAhead();
        return result;
    };
    // The lock is left over: say so, with the fix.
    auto staleLock = [&](Lock why) {
        if (why == Lock::Idle) {
            out.warn(Kyu::I18n::tr("pacman's lock looks left over: %1 is running but idle.").arg(idleHolder));
        } else if (why == Lock::Unseen) {
            out.warn(Kyu::I18n::tr("pacman's lock is there; Kyu cannot see other users' programs to tell who has it."));
        } else {
            out.warn(Kyu::I18n::tr("pacman's lock is left over from a stopped run; nothing is using it."));
        }
        Keys keys;
        if (!elevate.isEmpty()) {
            keys.fix = unlockCommand(elevate, lockPath);
            keys.fixShown = QStringLiteral("%1 rm %2").arg(elevate, lockPath);
            out.fix(keys.fixShown, Kyu::I18n::tr("Fix (if no pacman is running)"));
            // Minutes may pass before [f]: a package manager may have started working.
            keys.fixAllowed = [&out, lockPath, procPath]() {
                if (!QFileInfo::exists(lockPath)) {
                    return true;
                }
                // Any package manager now (a clock step can hide a holder's
                // age): one that cannot be idle, or that works, refuses.
                const QList<Manager> all = packageManagers(procPath);
                QHash<int, int> parents = parentMap(procPath);
                for (const Manager &m : all) {
                    if (!canBeIdle(m, all, parents)) {
                        out.warn(Kyu::I18n::tr("%1 is working now and may be using the lock: wait until it is done.").arg(describe(m)));
                        return false;
                    }
                }
                const QStringList before = cpuMarks(procPath, all, parents);
                QThread::msleep(1000);
                parents = parentMap(procPath);
                if (cpuMarks(procPath, all, parents) != before) {
                    out.warn(Kyu::I18n::tr("%1 is working now and may be using the lock: wait until it is done.").arg(describe(all.constFirst())));
                    return false;
                }
                return true;
            };
        }
        return finish(1, keys);
    };
    // The AUR helper installs with pacman: it needs the lock too.
    if ((!skipPacman || aurWanted) && QFileInfo::exists(lockPath)) {
        out.blank();
        switch (const Lock lock = waitForLock()) {
        case Lock::Free:
            break;
        case Lock::Stopped:
            out.warn(Kyu::I18n::tr("Stopped. No packages were upgraded."));
            return finish(4);
        case Lock::TimedOut:
            out.warn(Kyu::I18n::tr("Still busy after %1. Update when it is done.").arg(duration(lockLimit)));
            return finish(1);
        case Lock::Stale:
        case Lock::Idle:
        case Lock::Unseen:
            return staleLock(lock);
        }
    }

    if (!skipPacman && elevate.isEmpty()) {
        out.bad(Kyu::I18n::tr("Neither sudo, doas nor pkexec is installed: pacman needs one of them to run as root."));
        return finish(1);
    }

    // --- pacman ------------------------------------------------------------
    QElapsedTimer clock;
    clock.start();
    int pacmanExit = -1;
    Kyu::PacmanRun run; // everything pacman changed in this update
    bool lastStarted = false; // the last pacman command began changing packages
    QStringList keyringsFirst; // updated first, in a transaction of their own
    bool keyringsDone = false; // that command went through and the rest ran
    bool keyringsFailed = false; // that command failed
    QJsonObject failure;
    QStringList pacmanLines;
    const QString localDb = QFileInfo(lockPath).absolutePath() + QStringLiteral("/local");
    // pacman speaks the person's language: its failures are recognised with
    // its own translations, turned back into English (shown as printed).
    const Kyu::PacmanTranslations translations = Kyu::PacmanTranslations::forEnvironment(parser.value(localeDir));
    const QString logPath = parser.value(pacmanLog);
    // When pacman's package lists last changed (pacman gives each the
    // mirror's time when it downloads it): a sync that brought anything new.
    const QString syncDir = QFileInfo(lockPath).absolutePath() + QStringLiteral("/sync");
    const auto listsMark = [&syncDir]() {
        QStringList marks;
        const QDir dir(syncDir);
        for (const QFileInfo &db : dir.entryInfoList({QStringLiteral("*.db")}, QDir::Files, QDir::Name)) {
            marks.append(db.fileName() + QLatin1Char(' ') + QString::number(db.lastModified().toMSecsSinceEpoch()));
        }
        return marks;
    };
    const QStringList listsBefore = listsMark();
    struct PacmanStep {
        QString title;
        QStringList command;
    };
    // pacman: one command, or the keyrings and then the rest in one go (one
    // shell in one terminal session, so sudo asks for the password once).
    // Once more if pacman found the lock taken again (pamac's refresh, then
    // its upgrade) before changing anything. A value: the update ends here.
    const auto runPacman = [&](const QList<PacmanStep> &steps) -> std::optional<int> {
        out.blank();
        // The window closed meanwhile (the news, the lock): nothing is started.
        if (hungUp) {
            return finish(4);
        }
        out.step(steps.first().title, steps.first().command.join(QLatin1Char(' ')));
        for (int attempt = 0; attempt < 2; ++attempt) {
            const qint64 logOffset = QFileInfo(logPath).size();
            // util-linux `script` keeps pacman fully interactive (a terminal of
            // its own) and records what it prints, for the explanation of a
            // failure. The password is never echoed, so never recorded. It runs
            // the command with $SHELL: /bin/sh, whatever the login shell is.
            QTemporaryDir recording(recordingTemplate());
            const QString typescript = recording.filePath(QStringLiteral("pacman.typescript"));
            // Written when the second command starts: where pacman's log was then.
            const QString mark = recording.filePath(QStringLiteral("second"));
            QString shell = shellJoin(inhibited(steps.first().command));
            if (steps.size() > 1) {
                const QString log = shellJoin({logPath}), at = shellJoin({mark});
                shell += QStringLiteral(" && { wc -c 2>/dev/null <%1 >%2 || : >%2; %3; } && %4")
                             .arg(log, at, shellJoin({QStringLiteral("printf"), QStringLiteral("%s"), out.stepText(steps.at(1).title, steps.at(1).command.join(QLatin1Char(' ')))}),
                                  shellJoin(inhibited(steps.at(1).command)));
            }
            const QString script = QStandardPaths::findExecutable(QStringLiteral("script"));
            pacmanLines.clear();
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
            if (!script.isEmpty() && recording.isValid()) {
                pacmanExit = runInTerminal(script, {QStringLiteral("-q"), QStringLiteral("-e"), QStringLiteral("-f"), QStringLiteral("-c"), shell, typescript}, env,
                                           typescript);
                QFile ts(typescript);
                if (ts.open(QIODevice::ReadOnly)) {
                    pacmanLines = Kyu::terminalLines(ts.readAll());
                    // script's own first line ("Script started on …").
                    if (!pacmanLines.isEmpty() && pacmanLines.first().startsWith(QLatin1String("Script started"))) {
                        pacmanLines.removeFirst();
                    }
                }
            } else {
                pacmanExit = runInTerminal(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), shell}, env);
            }
            // The log, split where the second command started.
            const QByteArray appended = readFrom(logPath, logOffset);
            QByteArray firstLog = appended, secondLog;
            const bool second = steps.size() > 1 && QFileInfo::exists(mark);
            if (second) {
                QFile m(mark);
                bool known = false;
                const qint64 at = m.open(QIODevice::ReadOnly) ? m.readAll().trimmed().toLongLong(&known) - logOffset : -1;
                // Unknown (the log unreadable): all of it counts as the second
                // command's, which may then call the keyrings' transaction its.
                const qint64 split = known && at >= 0 && at <= appended.size() ? at : 0;
                firstLog = appended.left(split);
                secondLog = appended.mid(split);
            }
            const Kyu::PacmanRun first = Kyu::parsePacmanLog(QString::fromUtf8(firstLog));
            const Kyu::PacmanRun last = second ? Kyu::parsePacmanLog(QString::fromUtf8(secondLog)) : first;
            for (const Kyu::PacmanRun &part : second ? QList<Kyu::PacmanRun>{first, last} : QList<Kyu::PacmanRun>{first}) {
                run.transactionStarted = run.transactionStarted || part.transactionStarted;
                run.transactionCompleted = run.transactionCompleted || part.transactionCompleted;
                run.changed += part.changed;
                run.upgraded += part.upgraded;
                run.updated += part.updated;
                for (const QString &c : part.configs) {
                    if (!run.configs.contains(c)) {
                        run.configs.append(c);
                    }
                }
            }
            lastStarted = last.transactionStarted;
            if (steps.size() > 1) {
                keyringsDone = second;
                keyringsFailed = !second && pacmanExit != 0;
            }
            // The failing command's own lines: after Kyu's step line when the
            // second ran (the keyrings' run prints gpg's chatter).
            if (second) {
                const QString title = steps.at(1).title + QLatin1Char(' ');
                for (qsizetype n = pacmanLines.size() - 1; n >= 0; --n) {
                    if (pacmanLines.at(n).contains(title + steps.at(1).command.join(QLatin1Char(' ')))) {
                        pacmanLines = pacmanLines.mid(n + 1);
                        break;
                    }
                }
            }
            failure = pacmanExit != 0 ? Kyu::classifyFailure(translations.toEnglish(pacmanLines)) : QJsonObject();
            const bool lockTaken = failure.value(QStringLiteral("kind")).toString() == QLatin1String("lock") && !lastStarted;
            if (attempt > 0 || !lockTaken) {
                break;
            }
            out.blank();
            const Lock lock = waitForLock();
            if (lock == Lock::Stopped) {
                out.warn(run.changed.isEmpty() ? Kyu::I18n::tr("Stopped. No packages were upgraded.")
                                               : Kyu::I18n::tr("Stopped. Only %1 upgraded.").arg(run.changed.join(QStringLiteral(", "))));
                return finish(4);
            }
            if (lock == Lock::Stale || lock == Lock::Idle || lock == Lock::Unseen) {
                return staleLock(lock);
            }
            if (lock != Lock::Free) {
                break; // timed out: the verdict says why pacman stopped
            }
            out.step(steps.first().title, steps.first().command.join(QLatin1Char(' ')) + QLatin1Char(' ') + Kyu::I18n::tr("(again)"));
        }
        return std::nullopt;
    };
    if (!skipPacman) {
        // A keyring among the updates goes first, on its own: packages signed
        // with keys newer than the installed keyring fail otherwise (the
        // usual failure after a long gap). All installed keyrings, as Manjaro
        // advises; --needed skips current ones. The rest follows at once
        // (-Su), so this is not a partial upgrade. Not after the keyring fix
        // (--synced): it has just done this.
        if (state && !parser.isSet(synced)) {
            const QStringList installed = Kyu::installedKeyrings(localDb);
            const bool pending = std::any_of(state->packages.cbegin(), state->packages.cend(), [&installed](const Kyu::PackageUpdate &p) {
                return installed.contains(p.name);
            });
            if (pending) {
                keyringsFirst = installed;
            }
        }
        const QStringList hookDirs = parser.isSet(hooksDir) ? parser.values(hooksDir)
                                                            : QStringList{QStringLiteral("/usr/share/libalpm/hooks"), QStringLiteral("/etc/pacman.d/hooks")};
        if (const QString hook = snapshotHook(hookDirs); !hook.isEmpty()) {
            out.blank();
            out.quietStep(Kyu::I18n::tr("Snapshot"),
                          keyringsFirst.isEmpty() ? Kyu::I18n::tr("%1 takes one first").arg(hook)
                                                  : Kyu::I18n::tr("%1 takes one before the keyrings and one before the rest").arg(hook));
        }
        const bool refreshAll = parser.isSet(refresh);
        std::optional<int> end;
        if (!keyringsFirst.isEmpty()) {
            end = runPacman({{Kyu::I18n::tr("Keyrings"),
                              QStringList{elevate, QStringLiteral("pacman"), refreshAll ? QStringLiteral("-Syy") : QStringLiteral("-Sy"), QStringLiteral("--needed")}
                                  + keyringsFirst},
                             {Kyu::I18n::tr("Packages"), {elevate, QStringLiteral("pacman"), QStringLiteral("-Su")}}});
        } else {
            end = runPacman({{Kyu::I18n::tr("Packages"), {elevate, QStringLiteral("pacman"), refreshAll ? QStringLiteral("-Syyu") : QStringLiteral("-Syu")}}});
        }
        if (end) {
            return *end;
        }
    }

    // --- flatpak -------------------------------------------------------------
    int flatpakExit = -1;
    bool flatpakStopped = false;
    bool flatpakNothing = false;
    if ((skipPacman || pacmanExit == 0) && flatpakWanted && !hungUp) {
        out.blank();
        out.step(QStringLiteral("Flatpaks"), QStringLiteral("flatpak update"));
        QStringList shown;
        flatpakExit = runWatched({flatpak, QStringLiteral("update")}, QProcessEnvironment::systemEnvironment(), &shown);
        // "n" at flatpak's own question: a stop, not a failure. "Nothing to
        // update" though the check listed some: none were updated (a remote
        // gone or unreachable: flatpak only warns, and exits 0).
        // In any language: a choice ("[Y/n]", "[J/n]", "[O/n]") answered
        // with the letter that is not the default (the capital one).
        static const QRegularExpression choice(QStringLiteral("\\[(\\S{1,4})/(\\S{1,4})\\]:?\\s*(\\S*)\\s*$"));
        const auto declinedAt = [](const QString &l) {
            const auto m = choice.match(l);
            if (!m.hasMatch() || m.captured(3).isEmpty()) {
                return false;
            }
            const QString yes = m.captured(1), no = m.captured(2);
            const QString fallback = yes.at(0).isUpper() ? yes : no; // Enter's answer
            return m.captured(3).toLower() != fallback.toLower().left(m.captured(3).size());
        };
        flatpakStopped = flatpakExit > 0 && std::any_of(shown.cend() - qMin<qsizetype>(shown.size(), 3), shown.cend(), declinedAt);
        // flatpak lists what it updates by ID (never translated): none of
        // the listed ones named (its "F:" warnings aside), though the check
        // listed some, means none were updated (a remote gone: it only warns).
        flatpakNothing = flatpakExit == 0 && !shown.isEmpty() && state && !state->flatpaks.isEmpty()
            && std::none_of(state->flatpaks.cbegin(), state->flatpaks.cend(), [&shown](const Kyu::FlatpakUpdate &f) {
                   return std::any_of(shown.cbegin(), shown.cend(), [&f](const QString &l) {
                       return !l.startsWith(QLatin1String("F:")) && l.contains(f.name);
                   });
               });
    }

    // What is left to do comes from this run and, in the same boot, from
    // earlier ones: the kernel is checked every time.
    const QJsonObject previous = readJson(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
    const QString boot = bootId();
    const bool sameBoot = !boot.isEmpty() && previous.value(QStringLiteral("bootId")).toString() == boot;
    struct After {
        QString restartReason;
        bool restartFromThisRun = false;
        QStringList configs; // this run's, and earlier ones still waiting
    };
    const auto after = [&]() {
        After a;
        const Kyu::KernelState kernel = Kyu::kernelState(parser.value(procDir), parser.value(modulesDir));
        // In English: last-run.json's data, which the card reads;
        // Kyu::restartReasonText() says it in the user's language.
        if (kernel.replaced) {
            a.restartReason =kernel.installed.isEmpty() ? QStringLiteral("the running kernel %1 was removed").arg(kernel.running)
                                                         : QStringLiteral("new kernel %1").arg(kernel.installed);
            static const QRegularExpression kernelPackage(QStringLiteral("^linux([0-9]+|-lts|-zen|-hardened|-rt.*|-cachyos.*)?$"));
            for (const QString &name : std::as_const(run.updated)) {
                a.restartFromThisRun = a.restartFromThisRun || kernelPackage.match(name).hasMatch();
            }
        } else if (const QString pkg = Kyu::restartPackage(run); !pkg.isEmpty()) {
            a.restartReason = QStringLiteral("%1 was updated").arg(pkg);
            a.restartFromThisRun = true;
        } else if (sameBoot && previous.value(QStringLiteral("restartRequired")).toBool()) {
            a.restartReason = previous.value(QStringLiteral("restartReason")).toString();
            if (a.restartReason.endsWith(QLatin1Char('.'))) {
                a.restartReason.chop(1); // written by an earlier version
            }
        }
        a.configs = run.configs;
        for (const QString &path : strings(previous.value(QStringLiteral("configs")))) {
            if (!a.configs.contains(path) && QFileInfo::exists(path)) {
                a.configs.append(path);
            }
        }
        return a;
    };
    // pacman's last lines (not script's own), for a failure Kyu has no name for.
    const auto lastLines = [&pacmanLines]() {
        QJsonArray out;
        for (auto it = pacmanLines.crbegin(); it != pacmanLines.crend() && out.size() < 5; ++it) {
            const QString l = it->trimmed();
            if (!l.isEmpty() && !l.startsWith(QLatin1String("Script done"))) {
                out.prepend(l);
            }
        }
        return out;
    };
    // For the tray's card: what is left to do (last-run.json).
    const auto recordOf = [&](const After &a, const QStringList &programs, int aurExit, const QJsonArray &aurFailed) {
        return QJsonObject{
            {QStringLiteral("v"), 1},
            {QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {QStringLiteral("bootId"), boot},
            {QStringLiteral("pacmanExit"), pacmanExit < 0 ? QJsonValue() : QJsonValue(pacmanExit)},
            {QStringLiteral("flatpakExit"), flatpakExit < 0 ? QJsonValue() : QJsonValue(flatpakExit)},
            {QStringLiteral("aurExit"), aurExit < 0 ? QJsonValue() : QJsonValue(aurExit)},
            {QStringLiteral("aurFailed"), aurFailed},
            {QStringLiteral("changed"), int(run.changed.size())},
            {QStringLiteral("transactionStarted"), pacmanExit > 0 ? lastStarted : run.transactionStarted},
            {QStringLiteral("restartRequired"), !a.restartReason.isEmpty()},
            {QStringLiteral("restartReason"), a.restartReason},
            {QStringLiteral("configs"), QJsonArray::fromStringList(a.configs)},
            {QStringLiteral("programs"), QJsonArray::fromStringList(programs)},
            {QStringLiteral("failure"), failure.value(QStringLiteral("kind"))},
            // pacman's own lines about it, for the card's Details: the ones
            // the failure was recognised by, else pacman's last lines.
            {QStringLiteral("failureLines"), pacmanExit <= 0 ? QJsonValue() : failure.contains(QStringLiteral("lines")) ? failure.value(QStringLiteral("lines")) : lastLines()},
        };
    };
    const auto writeRecord = [](const QJsonObject &record) {
        QSaveFile last(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
        if (last.open(QIODevice::WriteOnly)) {
            last.write(QJsonDocument(record).toJson());
            last.commit();
        }
    };

    // --- AUR -----------------------------------------------------------------
    // Last, so a long build or a failed one holds nothing else up. What it
    // installed is in pacman's log too: a restart (an NVIDIA driver from the
    // AUR) and configuration files count.
    int aurExit = -1;
    Kyu::PacmanRun aurRun;
    // {name, version}: not updated last time, not offered again until a
    // newer version. Kept from earlier runs while still listed at that
    // version; replaced when the helper runs.
    QJsonArray aurFailed;
    for (const QJsonValue &v : previous.value(QStringLiteral("aurFailed")).toArray()) {
        const bool listed = state && std::any_of(state->aurPackages.cbegin(), state->aurPackages.cend(), [&v](const Kyu::PackageUpdate &p) {
            return p.name == v[QStringLiteral("name")].toString() && p.newVersion == v[QStringLiteral("version")].toString();
        });
        if (listed) {
            aurFailed.append(v);
        }
    }
    if ((skipPacman || pacmanExit == 0) && aurWanted && !hungUp) {
        // The system's part is done: saved now, and checked again, whatever
        // happens while the helper waits at its questions (or the window is
        // closed).
        writeRecord(recordOf(after(), {}, -1, aurFailed));
        if (!parser.isSet(noCheck)) {
            checkInBackground();
        }
        // The held-back ones left alone, as the helper's own "--ignore".
        QStringList command = aurHelper->command;
        if (!aurHeldBack.isEmpty()) {
            command << QStringLiteral("--ignore") << aurHeldBack.join(QLatin1Char(','));
        }
        out.blank();
        out.step(QStringLiteral("AUR"), command.join(QLatin1Char(' ')));
        out.note(Kyu::I18n::tr("%1 asks its own questions, and may ask for the password again.").arg(aurHelper->name), Tone::Dim);
        const qint64 logOffset = QFileInfo(logPath).size();
        const QStringList c = inhibited(command);
        aurExit = runWatched(c);
        aurRun = Kyu::parsePacmanLog(QString::fromUtf8(readFrom(logPath, logOffset)));
        // The held-back ones stay held back, whatever the helper did with
        // the rest (it was told to leave them alone).
        QJsonArray stillHeld;
        for (const QJsonValue &v : std::as_const(aurFailed)) {
            if (aurHeldBack.contains(v[QStringLiteral("name")].toString())) {
                stillHeld.append(v);
            }
        }
        aurFailed = stillHeld;
        if (aurExit > 0 && state) {
            for (const Kyu::PackageUpdate &p : state->aurPackages) {
                if (!aurRun.upgraded.contains(p.name) && !aurHeldBack.contains(p.name)) {
                    aurFailed.append(QJsonObject{{QStringLiteral("name"), p.name}, {QStringLiteral("version"), p.newVersion}});
                }
            }
        }
        run.updated += aurRun.updated;
        for (const QString &file : std::as_const(aurRun.configs)) {
            if (!run.configs.contains(file)) {
                run.configs.append(file);
            }
        }
    }

    // --- The verdict ---------------------------------------------------------
    const After now = after();
    const QString &restartReason = now.restartReason;
    const bool restartFromThisRun = now.restartFromThisRun;
    const QStringList &configs = now.configs;

    // Your own programs on replaced libraries (other users' are not readable
    // without root). Not needed when a restart is anyway.
    QStringList programs;
    if (restartReason.isEmpty() && (!run.changed.isEmpty() || !aurRun.changed.isEmpty())) {
        const QSet<int> mine = ancestors();
        for (const Kyu::OldLibraryProcess &p : Kyu::processesOnReplacedLibraries(::getuid(), parser.value(procDir))) {
            if (!mine.contains(p.pid) && !programs.contains(p.name)) {
                programs.append(p.name);
            }
        }
    }

    Keys keys;
    out.blank();
    // Stopped on purpose ("n", Ctrl+C, no password): not a failed update.
    bool stoppedOnPurpose = false;
    if (pacmanExit > 0) {
        const QString skipped = flatpakWanted && aurWanted ? QLatin1Char(' ') + Kyu::I18n::tr("Flatpaks and AUR packages were skipped.")
            : flatpakWanted                                 ? QLatin1Char(' ') + Kyu::I18n::tr("Flatpaks were skipped.")
            : aurWanted                                     ? QLatin1Char(' ') + Kyu::I18n::tr("AUR packages were skipped.")
                                                            : QString();
        // pacman's last lines, whatever the language; the last one is its
        // "::" question when it was answered no.
        QStringList last;
        for (auto it = pacmanLines.crbegin(); it != pacmanLines.crend() && last.size() < 3; ++it) {
            const QString l = it->trimmed();
            if (!l.isEmpty() && !l.startsWith(QLatin1String("Script done"))) {
                last.prepend(l);
            }
        }
        // Stopped on purpose, not failed: "n" at a question (pacman's last
        // line is its "::" question), Ctrl+C before anything changed (pacman
        // exits 130), or sudo (doas) not given the password: its own line,
        // "sudo: …" in any language; pacman never ran then.
        const bool pacmanSpoke = std::any_of(pacmanLines.cbegin(), pacmanLines.cend(), [](const QString &l) {
            return l.trimmed().startsWith(QLatin1String("::")) || l.trimmed().startsWith(QLatin1String("error:"));
        });
        const bool refused = !pacmanSpoke && !last.isEmpty()
            && (last.last().startsWith(QLatin1String("sudo: ")) || last.last().startsWith(QLatin1String("doas: ")))
            && !last.last().contains(QLatin1String("command not found"));
        // A question answered: "… [Y/n] n" ("[J/n]" in German); a "::" line
        // without one (a dependency pacman cannot satisfy) is not a stop.
        static const QRegularExpression answered(QStringLiteral("\\[[^\\[\\]/\\s]{1,4}/[^\\[\\]/\\s]{1,4}\\]\\s*\\S*\\s*$"));
        const bool declined = (!last.isEmpty() && last.last().startsWith(QLatin1String("::")) && answered.match(last.last()).hasMatch())
            || (pacmanExit == 130 && !lastStarted) || refused;
        stoppedOnPurpose = declined && !lastStarted;
        // What an earlier run of this update (the keyrings) installed.
        const QString only = run.changed.isEmpty()
            ? Kyu::I18n::tr("No packages were upgraded.")
            : Kyu::numbered("Only %1 were upgraded.", "Only %1 was upgraded.", run.changed.size()).arg(run.changed.join(QStringLiteral(", ")));
        if (lastStarted && pacmanExit > 128) {
            // Stopped by a signal (killed, the window closed) mid-transaction.
            out.bad((run.changed.isEmpty() ? Kyu::I18n::tr("The update was interrupted while pacman was changing packages.")
                                           : Kyu::numbered("The update was interrupted after %n packages had changed.",
                                                           "The update was interrupted after 1 package had changed.", run.changed.size()))
                    + skipped);
            out.note(Kyu::I18n::tr("Run the update again: pacman completes it."), Tone::Dim);
        } else if (lastStarted) {
            out.bad(Kyu::numbered("pacman failed after changing %n packages.", "pacman failed after changing 1 package.", run.changed.size()) + skipped);
        } else if (declined) {
            out.warn(Kyu::I18n::tr("Stopped.") + QLatin1Char(' ') + only + skipped);
            if (refused) {
                out.note(last.last(), Tone::Dim);
            }
        } else {
            out.bad((run.changed.isEmpty() ? QString() : Kyu::I18n::tr("pacman failed.") + QLatin1Char(' ')) + only + skipped);
        }
        // The package lists were synced and the upgrade did not begin:
        // installing anything now would be a partial upgrade. Synced: the
        // lists changed on disk, or by the keyring fix or the keyrings first,
        // or pacman got to a question or to a failure that comes after its
        // sync.
        const QString kind = failure.value(QStringLiteral("kind")).toString();
        static const QStringList afterSync{QStringLiteral("keyring"), QStringLiteral("mirror"), QStringLiteral("files"), QStringLiteral("disk"),
                                           QStringLiteral("dependencies")};
        const bool listsChanged = listsMark() != listsBefore;
        const bool partial = (parser.isSet(synced) || listsChanged || keyringsDone || (declined && !refused) || afterSync.contains(kind)) && !lastStarted;
        if (!failure.isEmpty()) {
            if (kind == QLatin1String("keyring")) {
                failure.insert(QStringLiteral("keyrings"), QJsonArray::fromStringList(Kyu::installedKeyrings(localDb)));
            }
            FailureText text = describeFailure(failure);
            // Updating the keyrings is what failed, or they are current now:
            // the usual fix does not help.
            if (kind == QLatin1String("keyring") && (keyringsDone || keyringsFailed || parser.isSet(synced))) {
                text.fix.clear();
                const QString root = elevate.isEmpty() ? QStringLiteral("sudo") : elevate;
                const QString keys = Kyu::I18n::tr("Check that the clock is right, then reload and refresh pacman's keys: %1, "
                                                   "then %2 (slow). pacman's messages above name the key.")
                                         .arg(root + QStringLiteral(" pacman-key --populate"), root + QStringLiteral(" pacman-key --refresh-keys"));
                text.text = (keyringsFailed ? Kyu::I18n::tr("The new keyring itself could not be checked: the installed one is too old to "
                                                            "trust it, which happens after a long gap.")
                                            : Kyu::I18n::tr("The keyrings are already current, so a newer keyring is not the answer."))
                    + QLatin1Char(' ') + keys;
            }
            out.note(text.title + QLatin1Char('.'));
            // Files and package files are not in the sentence: list them.
            if (kind == QLatin1String("files") || kind == QLatin1String("mirror") || kind == QLatin1String("mirrorlists")) {
                for (const QString &item : text.items.mid(0, 3)) {
                    out.note(QStringLiteral("· ") + item, Tone::Dim);
                }
                if (text.items.size() > 3) {
                    out.note(QStringLiteral("· ") + Kyu::numbered("and %n more", "and 1 more", text.items.size() - 3), Tone::Dim);
                }
            }
            // The fix with this system's sudo, doas or pkexec, and only with
            // the tools it needs.
            QString fix = text.fix;
            fix.replace(QRegularExpression(QStringLiteral("^sudo ")), elevate + QLatin1Char(' '));
            const QString tool = fix.section(QLatin1Char(' '), 1, 1);
            if (elevate.isEmpty() || (!tool.isEmpty() && QStandardPaths::findExecutable(tool).isEmpty())) {
                fix.clear();
            }
            if (!fix.isEmpty()) {
                keys.fix = fix;
                if (kind == QLatin1String("mirror") || kind == QLatin1String("mirrorlists")) {
                    keys.again.append(QStringLiteral("--refresh"));
                } else if (kind == QLatin1String("keyring")) {
                    keys.again.append(QStringLiteral("--synced"));
                }
                out.fix(keys.fix);
            } else {
                // No key for it: what to do, in words.
                for (const QString &line : wrap(text.text, 72)) {
                    out.note(line, Tone::Dim);
                }
            }
        } else if (!declined && !last.isEmpty()) {
            out.note(Kyu::I18n::tr("pacman's last lines (status %1):").arg(pacmanExit));
            for (const QString &l : last) {
                out.note(l, Tone::Dim);
            }
        }
        // After what went wrong and its fix: what not to do meanwhile.
        if (partial) {
            out.warn(Kyu::I18n::tr("The package lists are newer than the system: update before installing anything."));
        }
    } else {
        // AUR packages updated: those the check listed (not split, debug or
        // dependency packages, nor pamac's repo upgrades), when it listed them.
        qsizetype aurDone = aurRun.upgraded.size();
        if (state && state->aur.checked && state->aur.ok) {
            aurDone = std::count_if(state->aurPackages.cbegin(), state->aurPackages.cend(), [&aurRun](const Kyu::PackageUpdate &p) {
                return aurRun.upgraded.contains(p.name);
            });
        }
        QStringList parts;
        if (!run.changed.isEmpty()) {
            parts.append(Kyu::numbered("%n packages", "1 package", run.changed.size()));
        }
        if (aurDone > 0) {
            parts.append(Kyu::I18n::tr("%1 AUR").arg(aurDone));
        }
        if (flatpakExit == 0 && !flatpakNothing && state && !state->flatpaks.isEmpty()) {
            parts.append(Kyu::numbered("%n Flatpaks", "1 Flatpak", state->flatpaks.size()));
        }
        // How long it took, when that is worth saying.
        const QString done = (clock.elapsed() < 2000 ? Kyu::I18n::tr("Done") : Kyu::I18n::tr("Done in %1").arg(duration(clock.elapsed())))
            + QStringLiteral(" · ") + parts.join(QStringLiteral(" · "));
        const bool flatpakShort = flatpakNothing && state && !state->flatpaks.isEmpty();
        if (flatpakExit > 0 || aurExit > 0 || flatpakShort) {
            // What went through, then what did not.
            if (!parts.isEmpty()) {
                out.good(done);
            } else if (pacmanExit == 0) {
                out.good(Kyu::I18n::tr("The system is up to date."));
            }
            if (flatpakStopped) {
                out.warn(Kyu::I18n::tr("Flatpaks: stopped. None were updated."));
            } else if (flatpakExit > 0) {
                out.bad(Kyu::I18n::tr("Flatpaks failed (status %1), see above.").arg(flatpakExit));
            } else if (flatpakShort) {
                out.warn(Kyu::I18n::tr("Flatpaks not updated: flatpak found nothing to update (a remote it could not reach?), see above."));
            }
        } else {
            out.good(parts.isEmpty() ? Kyu::I18n::tr("Already up to date.") : done);
        }
    }

    // The AUR on its own line: never the system update's failure.
    if (aurExit > 0) {
        const bool restDone = flatpakExit <= 0 && (pacmanExit == 0 || flatpakExit == 0);
        out.warn(Kyu::I18n::tr("AUR packages not updated: %1 stopped (status %2), see above.").arg(aurHelper->name).arg(aurExit)
                 + (restDone ? QLatin1Char(' ') + Kyu::I18n::tr("The rest is done.") : QString()));
    }

    if (!restartReason.isEmpty()) {
        if (pacmanExit > 0 && lastStarted) {
            // Half an update: the new kernel's boot files may not be there.
            out.warn(Kyu::I18n::tr("Do not restart: the update is incomplete. Run it again first."));
        } else {
            // The reason is recorded in English (last-run.json), shown translated.
            out.warn((restartFromThisRun ? Kyu::I18n::tr("Restart required: %1") : Kyu::I18n::tr("Restart required: %1 (from an earlier update)"))
                         .arg(Kyu::restartReasonText(restartReason)));
            keys.restart = true;
        }
    }
    if (!configs.isEmpty()) {
        // The file the new version is for: /etc/sddm.conf for /etc/sddm.conf.pacnew.
        QStringList shown;
        for (const QString &path : configs) {
            shown.append(QString(path).remove(QRegularExpression(QStringLiteral("\\.pac(new|save)$"))));
        }
        out.warn(Kyu::numbered("%n configuration files to merge: %1", "1 configuration file to merge: %1", configs.size())
                     .arg(shown.mid(0, 3).join(QStringLiteral(", ")) + (shown.size() > 3 ? QStringLiteral(", …") : QString())));
        keys.configs = configs;
    }
    if (!programs.isEmpty()) {
        out.warn(Kyu::numbered("%1 still use old libraries: restart them.", "%1 still uses old libraries: restart it.", programs.size())
                     .arg(programs.mid(0, 6).join(QStringLiteral(", ")) + (programs.size() > 6 ? QStringLiteral(", …") : QString())));
    }

    const int code = pacmanExit > 0 ? pacmanExit : flatpakStopped ? 4 : flatpakExit > 0 ? 5 : aurExit > 0 ? 6 : 0;
    // After a whole update only: what can be cleaned up, as lines and a key
    // (never a question before the verdict; never on its own).
    if (code == 0 && !stoppedOnPurpose) {
        keys.house = Kyu::readHousekeeping();
        if (keys.house.any()) {
            printHousekeeping(out, keys.house);
        }
    }

    QJsonObject record = recordOf(now, programs, aurExit, aurFailed);
    // The card and the tray do not call a stop a failure.
    if (stoppedOnPurpose) {
        record.insert(QStringLiteral("stopped"), true);
    }
    auto save = [&record, writeRecord]() {
        writeRecord(record);
    };
    save();
    // After [m], the card lists only the files still waiting.
    keys.configsLeft = [&record, save](const QStringList &left) {
        record.insert(QStringLiteral("configs"), QJsonArray::fromStringList(left));
        save();
    };

    // The list is stale now: check again (in the background).
    if (!parser.isSet(noCheck)) {
        checkInBackground();
    }

    return finish(code, keys);
}
