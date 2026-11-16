// kyu-update: the update, in your terminal. Runs the real `sudo pacman -Syu`
// and then `flatpak update`, with their own output and their own questions
// ("Proceed?" included), and says what is left to do afterwards: a restart,
// settings files to review, programs still using replaced libraries, or why
// pacman failed and the command that usually fixes it.
//
// Before pacman: the distro news since the last full upgrade (and a
// "Continue? [y/N]" when there is any), and pacman's lock. Nothing here runs
// as root but pacman itself (through sudo, doas or pkexec).

#include "aftercare.h"
#include "failure.h"
#include "failuretext.h"
#include "paths.h"
#include "updatestate.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <csignal>
#include <cstdio>
#include <unistd.h>

namespace
{
enum class Tone { Plain, Info, Good, Warn, Bad };

class Out
{
public:
    Out()
        : m_color(::isatty(STDOUT_FILENO) && qEnvironmentVariableIsEmpty("NO_COLOR"))
    {
    }
    // "kyu  text", the prefix and first line in the tone's colour.
    void kyu(const QString &text, Tone tone = Tone::Info, bool bold = true)
    {
        print(QStringLiteral("kyu  ") + text, tone, bold);
    }
    // A continuation line under a kyu line.
    void more(const QString &text, Tone tone = Tone::Plain)
    {
        print(QStringLiteral("       ") + text, tone, false);
    }
    // The command about to run, as a prompt line.
    void command(const QString &text)
    {
        print(QStringLiteral("$ ") + text, Tone::Good, true);
    }
    void blank()
    {
        print(QString(), Tone::Plain, false);
    }
    void prompt(const QString &text)
    {
        write(QStringLiteral("kyu  ") + text, Tone::Warn, true);
        std::fflush(stdout);
    }

private:
    void print(const QString &text, Tone tone, bool bold)
    {
        write(text, tone, bold);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
    void write(const QString &text, Tone tone, bool bold)
    {
        if (!m_color || (tone == Tone::Plain && !bold)) {
            std::fputs(text.toLocal8Bit().constData(), stdout);
            return;
        }
        // Basic ANSI colours: the terminal's own palette (its colour scheme) draws them.
        const char *colour = tone == Tone::Info ? "34" : tone == Tone::Good ? "32" : tone == Tone::Warn ? "33" : tone == Tone::Bad ? "31" : "39";
        std::fprintf(stdout, "\x1b[%s%sm%s\x1b[0m", bold ? "1;" : "", colour, text.toLocal8Bit().constData());
    }
    bool m_color;
};

bool stdinIsTerminal()
{
    return ::isatty(STDIN_FILENO);
}

// One line from stdin, byte by byte: a buffered reader would also take the
// lines meant for pacman's own questions.
QString readAnswer()
{
    QByteArray line;
    char c = 0;
    while (::read(STDIN_FILENO, &c, 1) == 1 && c != '\n') {
        line.append(c);
    }
    return QString::fromLocal8Bit(line).trimmed().toLower();
}

// Run a command with the terminal: its output and questions are the user's.
// Ctrl+C reaches the command (same process group); Kyu itself keeps running
// to report what happened.
void noop(int) {}

int runInTerminal(const QString &program, const QStringList &args, const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment())
{
    struct sigaction quiet {};
    quiet.sa_handler = noop; // a handler, not SIG_IGN: exec resets it for the child
    struct sigaction oldInt {}, oldQuit {};
    ::sigaction(SIGINT, &quiet, &oldInt);
    ::sigaction(SIGQUIT, &quiet, &oldQuit);
    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.setInputChannelMode(QProcess::ForwardedInputChannel);
    process.setProcessEnvironment(env);
    process.start(program, args);
    int code = 127;
    if (process.waitForStarted(-1)) {
        process.waitForFinished(-1);
        code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 128 + 2;
    }
    ::sigaction(SIGINT, &oldInt, nullptr);
    ::sigaction(SIGQUIT, &oldQuit, nullptr);
    return code;
}

// A process named like a package manager that holds or will take pacman's lock.
QString runningPackageManager(const QString &procDir)
{
    for (const QString &entry : QDir(procDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        entry.toInt(&isPid);
        if (!isPid) {
            continue;
        }
        QFile comm(procDir + QLatin1Char('/') + entry + QStringLiteral("/comm"));
        if (!comm.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromUtf8(comm.readAll()).trimmed();
        // Exact names: the pamac tray ("pamac-tray-plas…") always runs and
        // takes no lock.
        static const QStringList managers{QStringLiteral("pacman"), QStringLiteral("pamac-daemon"), QStringLiteral("pamac-manager"),
                                          QStringLiteral("pamac-installer"), QStringLiteral("packagekitd")};
        if (managers.contains(name)) {
            return QStringLiteral("%1, pid %2").arg(name, entry);
        }
    }
    return {};
}

QByteArray readFrom(const QString &path, qint64 offset)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    // A log rotated meanwhile is shorter than before: read all of it.
    if (offset <= file.size()) {
        file.seek(offset);
    }
    return file.readAll();
}

QString plural(qsizetype n, const QString &one, const QString &many)
{
    return n == 1 ? QStringLiteral("1 ") + one : QStringLiteral("%1 ").arg(n) + many;
}

// Word-wraps a sentence for continuation lines.
QStringList wrap(const QString &text, int width = 64)
{
    QStringList lines;
    QString line;
    for (const QString &word : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (!line.isEmpty() && line.size() + 1 + word.size() > width) {
            lines.append(line);
            line.clear();
        }
        line += (line.isEmpty() ? QString() : QStringLiteral(" ")) + word;
    }
    if (!line.isEmpty()) {
        lines.append(line);
    }
    return lines;
}

// This process and the ones that started it (kyu-update, the shell, the
// terminal): they use replaced libraries too, after a Qt update, but are not
// what the user should restart.
QSet<int> ancestors()
{
    QSet<int> pids;
    int pid = ::getpid();
    while (pid > 1 && !pids.contains(pid)) {
        pids.insert(pid);
        QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
        if (!stat.open(QIODevice::ReadOnly)) {
            break;
        }
        // "pid (comm) state ppid ...": comm may contain spaces and parentheses.
        const QByteArray line = stat.readAll();
        const QList<QByteArray> rest = line.mid(line.lastIndexOf(')') + 2).split(' ');
        pid = rest.size() > 1 ? rest.at(1).toInt() : 0;
    }
    return pids;
}

QString bootId()
{
    QFile file(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

QStringList strings(const QJsonValue &value)
{
    QStringList out;
    for (const QJsonValue &v : value.toArray()) {
        out.append(v.toString());
    }
    return out;
}

QString checkBinary()
{
    const QString override = qEnvironmentVariable("KYU_CHECK");
    if (!override.isEmpty()) {
        return override;
    }
    const QString sibling = QCoreApplication::applicationDirPath() + QStringLiteral("/kyu-check");
    return QFileInfo(sibling).isExecutable() ? sibling : QStringLiteral("kyu-check");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kyu-update"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Updates the system in this terminal: sudo pacman -Syu, then flatpak update, "
                                                    "with the news before and what is left to do after."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noWait(QStringLiteral("no-wait"), QStringLiteral("Do not wait for Enter at the end."));
    QCommandLineOption noCheck(QStringLiteral("no-check"), QStringLiteral("Do not run kyu-check afterwards."));
    QCommandLineOption stateFile(QStringLiteral("state-file"), QStringLiteral("kyu-check's state (testing)."), QStringLiteral("path"));
    QCommandLineOption pacmanLog(QStringLiteral("pacman-log"), QStringLiteral("pacman's log (testing)."), QStringLiteral("path"),
                                 QStringLiteral("/var/log/pacman.log"));
    QCommandLineOption dbLock(QStringLiteral("db-lock"), QStringLiteral("pacman's lock file (testing)."), QStringLiteral("path"),
                              QStringLiteral("/var/lib/pacman/db.lck"));
    QCommandLineOption procDir(QStringLiteral("proc-dir"), QStringLiteral("/proc (testing)."), QStringLiteral("path"), QStringLiteral("/proc"));
    QCommandLineOption modulesDir(QStringLiteral("modules-dir"), QStringLiteral("/usr/lib/modules (testing)."), QStringLiteral("path"),
                                  QStringLiteral("/usr/lib/modules"));
    parser.addOptions({noWait, noCheck, stateFile, pacmanLog, dbLock, procDir, modulesDir});
    parser.process(app);

    Out out;
    QDir().mkpath(Kyu::Paths::stateDir());
    QLockFile lock(Kyu::Paths::stateDir() + QStringLiteral("/update.lock"));
    // The end: the lock goes first (the tray's card stops saying "Updating"),
    // then the window waits, so a printed command can still be typed: Enter
    // closes it, "s" opens a shell here.
    // The exit status says what happened (0 done, 1 pacman failed, 3 already
    // running, 4 stopped at the news, 5 flatpak failed) only without the wait
    // (--no-wait, or no terminal): after it the person has read the result,
    // last-run.json has it, and a non-zero status would only make Konsole
    // post a "Program exited with status" notification.
    auto finish = [&](int code) {
        lock.unlock();
        if (!parser.isSet(noWait) && stdinIsTerminal()) {
            out.blank();
            out.prompt(QStringLiteral("Press Enter to close, or type s and Enter for a shell here. "));
            if (readAnswer() == QLatin1String("s")) {
                const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
                runInTerminal(shell, {});
            }
            return 0;
        }
        return code;
    };

    // One update at a time; the tray's card reads this lock too.
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        QString host, appName;
        lock.getLockInfo(&pid, &host, &appName);
        out.kyu(QStringLiteral("An update is already running (pid %1).").arg(pid), Tone::Warn);
        return parser.isSet(noWait) || !stdinIsTerminal() ? 3 : (out.prompt(QStringLiteral("Press Enter to close. ")), readAnswer(), 0);
    }

    // --- Before pacman ----------------------------------------------------
    const QString statePath = parser.isSet(stateFile) ? parser.value(stateFile) : Kyu::Paths::stateFile();
    const std::optional<Kyu::UpdateState> state = Kyu::UpdateState::load(statePath);
    if (state && state->checkedAt.isValid()) {
        out.kyu(QStringLiteral("%1: %2, %3")
                    .arg(plural(state->total(), QStringLiteral("update"), QStringLiteral("updates")),
                         plural(state->packages.size(), QStringLiteral("package"), QStringLiteral("packages")),
                         plural(state->flatpaks.size(), QStringLiteral("Flatpak"), QStringLiteral("Flatpaks"))));
    } else {
        out.kyu(QStringLiteral("Updating. (Kyu has not checked for updates yet.)"));
    }

    if (state && !state->news.items.isEmpty()) {
        out.kyu(QStringLiteral("%1 since your last upgrade:").arg(state->news.source), Tone::Warn);
        for (const Kyu::NewsItem &item : state->news.items.mid(0, 5)) {
            out.more(item.title, Tone::Warn);
            out.more(item.link);
        }
        if (state->news.items.size() > 5) {
            out.more(QStringLiteral("… and %1 more: %2").arg(state->news.items.size() - 5).arg(state->news.page));
        }
        // No by default: Enter alone must not skip news that may need action.
        out.prompt(QStringLiteral("Continue? [y/N] "));
        const QString answer = readAnswer();
        if (!stdinIsTerminal()) {
            out.blank(); // a typed answer ends the line itself
        }
        if (answer != QLatin1String("y") && answer != QLatin1String("yes")) {
            out.kyu(QStringLiteral("Stopped. Nothing was changed."));
            return finish(4);
        }
    }

    // pacman's lock: another package manager, or one left over from a run that was stopped.
    if (QFileInfo::exists(parser.value(dbLock))) {
        const QString holder = runningPackageManager(parser.value(procDir));
        if (!holder.isEmpty()) {
            out.kyu(QStringLiteral("Another package manager is running (%1). Update when it has finished.").arg(holder), Tone::Warn);
        } else {
            out.kyu(QStringLiteral("pacman's lock file is left over from a run that was stopped:"), Tone::Warn);
            out.more(parser.value(dbLock));
            out.more(QStringLiteral("No package manager is running, so it is safe to remove it:"));
            out.more(QStringLiteral("sudo rm ") + parser.value(dbLock), Tone::Info);
        }
        return finish(1);
    }

    // Only Flatpaks pending (by the last check): no sudo, no pacman.
    const bool flatpakOnly = state && state->pacman.checked && state->pacman.ok && state->packages.isEmpty() && !state->flatpaks.isEmpty();
    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    const bool flatpakWanted = !flatpak.isEmpty() && (!state || !state->flatpak.checked || !state->flatpak.ok || !state->flatpaks.isEmpty());

    QString elevate;
    if (!flatpakOnly) {
        for (const char *tool : {"sudo", "doas", "pkexec"}) {
            if (!QStandardPaths::findExecutable(QLatin1String(tool)).isEmpty()) {
                elevate = QLatin1String(tool);
                break;
            }
        }
        if (elevate.isEmpty()) {
            out.kyu(QStringLiteral("Neither sudo, doas nor pkexec is installed: pacman needs one of them to run as root."), Tone::Bad);
            return finish(1);
        }
    }
    out.kyu(QStringLiteral("Do not close this window until Kyu says it is done."));
    out.blank();

    // --- pacman ------------------------------------------------------------
    int pacmanExit = -1;
    Kyu::PacmanRun run;
    QJsonObject failure;
    QStringList pacmanLines;
    if (!flatpakOnly) {
        const QString command = elevate + QStringLiteral(" pacman -Syu");
        out.command(command);
        const qint64 logOffset = QFileInfo(parser.value(pacmanLog)).size();
        // util-linux `script` keeps pacman fully interactive (a terminal of
        // its own) and records what it prints, for the explanation of a
        // failure. The password is never echoed, so never recorded. It runs
        // the command with $SHELL: /bin/sh, whatever the login shell is.
        QTemporaryDir recording;
        const QString script = QStandardPaths::findExecutable(QStringLiteral("script"));
        const QString typescript = recording.filePath(QStringLiteral("pacman.typescript"));
        if (!script.isEmpty() && recording.isValid()) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
            pacmanExit = runInTerminal(script, {QStringLiteral("-q"), QStringLiteral("-e"), QStringLiteral("-f"), QStringLiteral("-c"), command, typescript}, env);
            QFile ts(typescript);
            if (ts.open(QIODevice::ReadOnly)) {
                pacmanLines = Kyu::terminalLines(ts.readAll());
                // script's own first and last lines ("Script started/done on …").
                if (!pacmanLines.isEmpty() && pacmanLines.first().startsWith(QLatin1String("Script started"))) {
                    pacmanLines.removeFirst();
                }
            }
        } else {
            pacmanExit = runInTerminal(elevate, {QStringLiteral("pacman"), QStringLiteral("-Syu")});
        }
        run = Kyu::parsePacmanLog(QString::fromUtf8(readFrom(parser.value(pacmanLog), logOffset)));
        if (pacmanExit != 0) {
            failure = Kyu::classifyFailure(pacmanLines);
        }
    }

    // --- flatpak -------------------------------------------------------------
    int flatpakExit = -1;
    if ((flatpakOnly || pacmanExit == 0) && flatpakWanted) {
        out.blank();
        out.command(QStringLiteral("flatpak update"));
        flatpakExit = runInTerminal(flatpak, {QStringLiteral("update")});
    }

    // --- After ---------------------------------------------------------------
    out.blank();
    if (pacmanExit > 0) {
        const QString flatpakNote = flatpakWanted ? QStringLiteral(" Flatpaks were not updated.") : QString();
        if (run.transactionStarted) {
            out.kyu(QStringLiteral("pacman failed (exit status %1) after changing %2.")
                            .arg(pacmanExit)
                            .arg(plural(run.changed.size(), QStringLiteral("package"), QStringLiteral("packages")))
                        + flatpakNote,
                    Tone::Bad);
        } else if (failure.isEmpty()) {
            // A wrong password, "n" at pacman's question, or an error Kyu does
            // not know (or in another language): nothing changed either way.
            out.kyu(QStringLiteral("pacman stopped before changing anything (exit status %1).").arg(pacmanExit) + flatpakNote, Tone::Warn);
        } else {
            out.kyu(QStringLiteral("pacman failed (exit status %1). No packages were changed.").arg(pacmanExit) + flatpakNote, Tone::Bad);
        }
        if (!failure.isEmpty()) {
            const FailureText text = describeFailure(failure);
            out.kyu(text.title + QLatin1Char('.'), Tone::Warn);
            for (const QString &line : wrap(text.text)) {
                out.more(line);
            }
            // Files and package files are not in the sentence: list them.
            const QString kind = failure.value(QStringLiteral("kind")).toString();
            if (kind == QLatin1String("files") || kind == QLatin1String("mirror")) {
                for (const QString &item : text.items.mid(0, 5)) {
                    out.more(QStringLiteral("· ") + item);
                }
                if (text.items.size() > 5) {
                    out.more(QStringLiteral("· and %1 more").arg(text.items.size() - 5));
                }
            }
            if (!text.command.isEmpty()) {
                out.kyu(QStringLiteral("This usually fixes it:"), Tone::Info);
                out.more(text.command, Tone::Info);
            }
        } else {
            // pacman's last error lines, whatever the language.
            QStringList last;
            for (auto it = pacmanLines.crbegin(); it != pacmanLines.crend() && last.size() < 3; ++it) {
                const QString l = it->trimmed();
                if (!l.isEmpty() && !l.startsWith(QLatin1String("Script done"))) {
                    last.prepend(l);
                }
            }
            // Not after "n" at pacman's own question (its last line is the "::" prompt).
            const bool declined = !last.isEmpty() && last.last().startsWith(QLatin1String("::"));
            if (!last.isEmpty() && !declined) {
                out.kyu(QStringLiteral("pacman's last lines:"), Tone::Plain, false);
                for (const QString &l : last) {
                    out.more(l);
                }
            }
        }
    } else {
        QStringList done;
        if (pacmanExit == 0 && !run.changed.isEmpty()) {
            done.append(plural(run.changed.size(), QStringLiteral("package"), QStringLiteral("packages")));
        }
        if (flatpakExit == 0) {
            done.append(QStringLiteral("flatpak update finished"));
        }
        if (flatpakExit > 0) {
            out.kyu(QStringLiteral("flatpak update failed (exit status %1); its own messages are above.").arg(flatpakExit), Tone::Bad);
            if (!run.changed.isEmpty()) {
                out.kyu(QStringLiteral("pacman's part is done: %1.").arg(plural(run.changed.size(), QStringLiteral("package"), QStringLiteral("packages"))),
                        Tone::Good);
            }
        } else {
            out.kyu(done.isEmpty() ? QStringLiteral("Done: nothing new.") : QStringLiteral("Done: %1.").arg(done.join(QStringLiteral(", "))), Tone::Good);
        }
    }

    // What is left to do, after any run: the kernel is checked every time,
    // and what an earlier run left (same boot) is not forgotten.
    const QJsonObject previous = readJson(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
    const QString boot = bootId();
    const bool sameBoot = !boot.isEmpty() && previous.value(QStringLiteral("bootId")).toString() == boot;
    QString restartReason;
    const Kyu::KernelState kernel = Kyu::kernelState(parser.value(procDir), parser.value(modulesDir));
    if (kernel.replaced) {
        restartReason = kernel.installed.isEmpty() ? QStringLiteral("the running kernel %1 was replaced.").arg(kernel.running)
                                                   : QStringLiteral("kernel %1 running, %2 installed.").arg(kernel.running, kernel.installed);
    } else if (const QString pkg = Kyu::restartPackage(run); !pkg.isEmpty()) {
        restartReason = QStringLiteral("%1 was updated.").arg(pkg);
    } else if (sameBoot && previous.value(QStringLiteral("restartRequired")).toBool()) {
        restartReason = previous.value(QStringLiteral("restartReason")).toString();
    }
    if (!restartReason.isEmpty()) {
        out.kyu(QStringLiteral("Restart needed: ") + restartReason, Tone::Warn);
    }

    // Settings files: this run's, and earlier ones still waiting.
    QStringList configs = run.configs;
    for (const QString &path : strings(previous.value(QStringLiteral("configs")))) {
        if (!configs.contains(path) && QFileInfo::exists(path)) {
            configs.append(path);
        }
    }
    if (!configs.isEmpty()) {
        out.kyu(plural(configs.size(), QStringLiteral("settings file"), QStringLiteral("settings files")) + QStringLiteral(" to review:"), Tone::Info);
        for (const QString &path : configs) {
            out.more(path);
        }
        out.more(QStringLiteral("compare and merge: sudo pacdiff"), Tone::Info);
    }

    // Your own programs on replaced libraries (other users' are not readable
    // without root). Not needed when a restart is anyway.
    QStringList programs;
    if (restartReason.isEmpty() && !run.changed.isEmpty()) {
        const QSet<int> mine = ancestors();
        for (const Kyu::OldLibraryProcess &p : Kyu::processesOnReplacedLibraries(::getuid(), parser.value(procDir))) {
            if (!mine.contains(p.pid) && !programs.contains(p.name)) {
                programs.append(p.name);
            }
        }
        if (!programs.isEmpty()) {
            out.kyu(QStringLiteral("Programs you run still use replaced libraries: %1. Restart them, or log out and in.")
                        .arg(programs.mid(0, 6).join(QStringLiteral(", ")) + (programs.size() > 6 ? QStringLiteral(", …") : QString())),
                    Tone::Info, false);
        }
    }

    // For the tray's card: what is left to do.
    QJsonObject record{
        {QStringLiteral("v"), 1},
        {QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("bootId"), boot},
        {QStringLiteral("pacmanExit"), pacmanExit < 0 ? QJsonValue() : QJsonValue(pacmanExit)},
        {QStringLiteral("flatpakExit"), flatpakExit < 0 ? QJsonValue() : QJsonValue(flatpakExit)},
        {QStringLiteral("changed"), int(run.changed.size())},
        {QStringLiteral("restartRequired"), !restartReason.isEmpty()},
        {QStringLiteral("restartReason"), restartReason},
        {QStringLiteral("configs"), QJsonArray::fromStringList(configs)},
        {QStringLiteral("programs"), QJsonArray::fromStringList(programs)},
        {QStringLiteral("failure"), failure.value(QStringLiteral("kind"))},
    };
    QSaveFile last(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
    if (last.open(QIODevice::WriteOnly)) {
        last.write(QJsonDocument(record).toJson());
        last.commit();
    }

    // The list is stale now: check again (in the background).
    if (!parser.isSet(noCheck)) {
        QProcess::startDetached(checkBinary(), {});
    }

    const int code = pacmanExit > 0 ? pacmanExit : flatpakExit > 0 ? 5 : 0;
    return finish(code);
}
