#include "console.h"

namespace KyuUpdate
{
int stepWidth()
{
    // Every step name Kyu translates; Flatpak(s) and AUR stay as they are.
    static const char *const names[] = {QT_TRANSLATE_NOOP("Kyu", "Keyrings"), QT_TRANSLATE_NOOP("Kyu", "Packages"),
                                        QT_TRANSLATE_NOOP("Kyu", "Orphans"), QT_TRANSLATE_NOOP("Kyu", "Cache"),
                                        QT_TRANSLATE_NOOP("Kyu", "Snapshot")};
    int width = 10;
    for (const char *name : names) {
        width = std::max(width, int(QCoreApplication::translate("Kyu", name).size()));
    }
    return width;
}

bool stdinIsTerminal()
{
    return ::isatty(STDIN_FILENO);
}

// Keys typed before a question are not its answer.
void discardTypedAhead()
{
    if (::isatty(STDIN_FILENO)) {
        ::tcflush(STDIN_FILENO, TCIFLUSH);
    }
}

volatile std::sig_atomic_t answerInterrupted = 0;
void onAnswerInterrupt(int)
{
    answerInterrupted = 1;
}

// The terminal closed (its window): Kyu stays to record what happened;
// nothing more is run, shown or asked.
volatile std::sig_atomic_t hungUp = 0;
void onHangup(int)
{
    hungUp = 1;
}

// One line from stdin, byte by byte: a buffered reader would also take the
// lines meant for pacman's own questions. Ctrl+C at Kyu's own question is
// "no" (or "close"), not Kyu killed with its marks left behind.
QString readAnswer()
{
    struct sigaction stop {}, old {};
    stop.sa_handler = onAnswerInterrupt; // no SA_RESTART: the read returns
    answerInterrupted = 0;
    ::sigaction(SIGINT, &stop, &old);
    QByteArray line;
    char c = 0;
    bool ended = false;
    for (;;) {
        const ssize_t n = ::read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            if (c == '\n') {
                ended = true;
                break;
            }
            line.append(c);
        } else if (n < 0 && errno == EINTR && !answerInterrupted && !hungUp) {
            continue; // another signal: keep reading
        } else {
            break; // end of input, Ctrl+C, the window closed
        }
    }
    ::sigaction(SIGINT, &old, nullptr);
    if (answerInterrupted) {
        std::fputs("\n", stdout);
        std::fflush(stdout);
    }
    // Only a whole line is an answer: "y" typed, then the window closed
    // (or Ctrl+C), is no answer, not a yes to a restart.
    if (!ended || answerInterrupted || hungUp) {
        return QString();
    }
    return QString::fromLocal8Bit(line).trimmed().toLower();
}

// The keys stay [y/N] in every language; "j" and "ja" are what a German
// hand types anyway.
bool isYes(const QString &answer)
{
    const QString a = answer.trimmed().toLower();
    return a == QLatin1String("y") || a == QLatin1String("yes") || a == QLatin1String("j") || a == QLatin1String("ja");
}

// Run a command with the terminal: its output and questions are the user's.
// Ctrl+C reaches the command (same process group); Kyu itself keeps running
// to report what happened.
void noop(int) {}

// watch: the file `script` records the command's output to. While the
// output stands still on a question (pacman's "Proceed?", sudo's password),
// the state directory says the update is waiting (waiting.h), for the Kyu
// window to tell when it is not in front.
int runInTerminal(const QString &program, const QStringList &args, const QProcessEnvironment &env,
                  const QString &watch)
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
        if (watch.isEmpty()) {
            process.waitForFinished(-1);
        } else {
            qint64 size = -1;
            QElapsedTimer still;
            still.start();
            QString shown;
            while (!process.waitForFinished(500)) {
                QFile recorded(watch);
                const qint64 now = recorded.size();
                if (now != size) {
                    size = now;
                    still.restart();
                    if (!shown.isEmpty()) {
                        shown.clear();
                        Kyu::setWaiting(QString());
                    }
                } else if (shown.isEmpty() && still.elapsed() >= 1500 && recorded.open(QIODevice::ReadOnly)) {
                    recorded.seek(qMax<qint64>(0, now - 4096));
                    shown = Kyu::waitingPrompt(recorded.readAll());
                    if (!shown.isEmpty()) {
                        Kyu::setWaiting(shown);
                    }
                }
            }
            Kyu::setWaiting(QString());
        }
        code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 128 + 2;
    }
    ::sigaction(SIGINT, &oldInt, nullptr);
    ::sigaction(SIGQUIT, &oldQuit, nullptr);
    return code;
}

// Processes named like a package manager that takes pacman's lock.

QList<Manager> packageManagers(const QString &procDir)
{
    // Exact names: the pamac tray ("pamac-tray-plas…") always runs and
    // takes no lock.
    static const QStringList managers{QStringLiteral("pacman"),         QStringLiteral("pamac"),           QStringLiteral("pamac-daemon"),
                                      QStringLiteral("pamac-manager"), QStringLiteral("pamac-installer"), QStringLiteral("packagekitd")};
    QList<Manager> found;
    for (const QString &entry : QDir(procDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const int pid = entry.toInt(&isPid);
        if (!isPid) {
            continue;
        }
        QFile comm(procDir + QLatin1Char('/') + entry + QStringLiteral("/comm"));
        if (!comm.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromUtf8(comm.readAll()).trimmed();
        if (managers.contains(name)) {
            found.append({name, pid});
        }
    }
    return found;
}

QString describe(const Manager &m)
{
    return QStringLiteral("%1 (pid %2)").arg(m.name).arg(m.pid);
}

// From /proc/<pid>/stat: the fields after "(comm)", 0-based from field 3.
QStringList statFields(const QString &procDir, int pid)
{
    QFile file(QStringLiteral("%1/%2/stat").arg(procDir).arg(pid));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray line = file.readAll();
    return QString::fromLatin1(line.mid(line.lastIndexOf(')') + 2)).split(QLatin1Char(' '));
}

// When a process started (seconds since the epoch); 0 when unknown.
qint64 processStart(const QString &procDir, int pid)
{
    const QStringList f = statFields(procDir, pid);
    QFile stat(procDir + QStringLiteral("/stat"));
    if (f.size() < 20 || !stat.open(QIODevice::ReadOnly)) {
        return 0;
    }
    qint64 boot = 0;
    for (const QByteArray &line : stat.readAll().split('\n')) {
        if (line.startsWith("btime ")) {
            boot = line.mid(6).trimmed().toLongLong();
        }
    }
    const long ticks = ::sysconf(_SC_CLK_TCK);
    return boot > 0 && ticks > 0 ? boot + f.at(19).toLongLong() / ticks : 0;
}

// CPU time used so far (clock ticks); -1 when unknown.
qint64 cpuTicks(const QString &procDir, int pid)
{
    const QStringList f = statFields(procDir, pid);
    return f.size() > 12 ? f.at(11).toLongLong() + f.at(12).toLongLong() : -1;
}

// The package managers that can hold the lock: those started before it was
// taken (libalpm creates it anew each time, so its time is when).
QList<Manager> lockHolders(const QString &procDir, const QString &lockPath)
{
    const qint64 taken = QFileInfo(lockPath).lastModified().toSecsSinceEpoch();
    QList<Manager> holders;
    for (const Manager &m : packageManagers(procDir)) {
        const qint64 start = processStart(procDir, m.pid);
        if (start == 0 || start <= taken) {
            holders.append(m);
        }
    }
    return holders;
}

// Every process's parent, from /proc/<pid>/stat.
QHash<int, int> parentMap(const QString &procDir)
{
    QHash<int, int> parents;
    for (const QString &entry : QDir(procDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const int pid = entry.toInt(&isPid);
        if (isPid) {
            if (const QStringList f = statFields(procDir, pid); f.size() > 1) {
                parents.insert(pid, f.at(1).toInt());
            }
        }
    }
    return parents;
}

// A process's descendants (hooks, dkms, mkinitcpio run as children).
QList<int> descendants(const QHash<int, int> &parents, int pid)
{
    QList<int> found;
    for (auto it = parents.cbegin(); it != parents.cend(); ++it) {
        for (int p = it.value(), hops = 0; p > 1 && hops < 64; p = parents.value(p, 0), ++hops) {
            if (p == pid) {
                found.append(it.key());
                break;
            }
        }
    }
    return found;
}

// Whether a package manager may be called idle when nothing changes: never
// pacman itself (it may sit at its own question; it exits when done), never
// one with children (a long hook), never pamac's daemon while pamac's window
// is open (a confirmation or password dialog).
bool canBeIdle(const Manager &m, const QList<Manager> &all, const QHash<int, int> &parents)
{
    if (m.name == QLatin1String("pacman") || m.name == QLatin1String("pamac") || !descendants(parents, m.pid).isEmpty()) {
        return false;
    }
    if (m.name == QLatin1String("pamac-daemon")) {
        for (const Manager &other : all) {
            if (other.name == QLatin1String("pamac") || other.name == QLatin1String("pamac-manager") || other.name == QLatin1String("pamac-installer")) {
                return false;
            }
        }
    }
    return true;
}

// What changes while package managers work: their CPU time and their
// children's (several numbers; compared as a whole).
QStringList cpuMarks(const QString &procDir, const QList<Manager> &managers, const QHash<int, int> &parents)
{
    QStringList marks;
    for (const Manager &m : managers) {
        marks.append(QString::number(cpuTicks(procDir, m.pid)));
        for (int child : descendants(parents, m.pid)) {
            marks.append(QString::number(cpuTicks(procDir, child)));
        }
    }
    return marks;
}


// The [f] for a left-over lock, run as root: removes the lock only if no
// program has it open (libalpm keeps it open as long as it holds it; root
// can see every process's open files), else says which program has it.
// Kyu's own guesses only decide when to offer it.
QString unlockCommand(const QString &elevate, const QString &lockPath)
{
    // One find over all open files (a desktop has tens of thousands); the
    // lock's real path, as /proc shows it (/var may be a symlink).
    static const QString script = QStringLiteral(
        "l=$(readlink -f -- \"$1\"); "
        "f=$(find /proc/[0-9]*/fd -lname \"$l\" -print -quit 2>/dev/null); "
        "if [ -n \"$f\" ]; then p=${f#/proc/}; p=${p%%/*}; "
        "echo \"pacman's lock is in use by $(cat /proc/$p/comm 2>/dev/null) (pid $p); not removed.\"; exit 1; fi; "
        "rm -f -- \"$1\"");
    return shellJoin({elevate, QStringLiteral("sh"), QStringLiteral("-c"), script, QStringLiteral("kyu-unlock"), lockPath});
}

// Whether other users' processes are visible (/proc mounted without hidepid).
bool canSeeAllProcesses(const QString &procDir)
{
    return QFileInfo(procDir + QStringLiteral("/1/comm")).isReadable();
}

// Set by Ctrl+C while waiting for the lock.
volatile std::sig_atomic_t interrupted = 0;
void onInterrupt(int)
{
    interrupted = 1;
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

// Word-wraps a sentence for continuation lines.
QStringList wrap(const QString &text, int width)
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

// A pacman hook that takes a snapshot before every transaction, by name.
QString snapshotHook(const QStringList &dirs)
{
    for (const QString &dir : dirs) {
        for (const QString &name : QDir(dir).entryList({QStringLiteral("*.hook")}, QDir::Files | QDir::System)) {
            // A hook of the same name that is a link to /dev/null, in any of
            // the directories, switches it off.
            bool masked = false;
            for (const QString &other : dirs) {
                masked = masked || QFileInfo(other + QLatin1Char('/') + name).symLinkTarget() == QLatin1String("/dev/null");
            }
            if (masked) {
                continue;
            }
            if (name.contains(QLatin1String("snap-pac"))) {
                return QStringLiteral("snap-pac");
            }
            if (name.contains(QLatin1String("timeshift-autosnap"))) {
                return QStringLiteral("timeshift-autosnap");
            }
        }
    }
    return {};
}

QString duration(qint64 ms)
{
    const qint64 s = (ms + 500) / 1000;
    return s < 60 ? Kyu::I18n::tr("%1 s").arg(s) : Kyu::I18n::tr("%1 min").arg((s + 30) / 60);
}

// The command to run pacman with, kept from sleeping where logind allows it.
QStringList inhibited(const QStringList &command)
{
    const QString inhibit = QStandardPaths::findExecutable(QStringLiteral("systemd-inhibit"));
    if (inhibit.isEmpty()) {
        return command;
    }
    const QStringList args{QStringLiteral("--no-ask-password"), QStringLiteral("--what=idle:sleep:shutdown"), QStringLiteral("--who=Kyu"),
                           // Plasma's battery and brightness applet shows why.
                           QStringLiteral("--why=") + Kyu::I18n::tr("Updating packages"), QStringLiteral("--mode=block")};
    QProcess probe;
    probe.setStandardInputFile(QProcess::nullDevice());
    probe.start(inhibit, args + QStringList{QStringLiteral("true")});
    if (!probe.waitForFinished(5000) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        return command;
    }
    return QStringList{inhibit} + args + command;
}

// A command line for `script -c` (run by /bin/sh).
// Where `script` records a command's output: on disk in Kyu's state
// directory, not in /tmp (RAM on Arch; an AUR build prints a lot).
QString recordingTemplate()
{
    QDir().mkpath(Kyu::Paths::stateDir());
    return Kyu::Paths::stateDir() + QStringLiteral("/recording-XXXXXX");
}

// A command that may ask questions (flatpak, the AUR helper), run inside
// util-linux `script` so its output can be watched for them; as it is
// without `script`, or when nobody is at a terminal to answer.
// shown: if given, what the command showed last (its last lines, as the
// screen had them; empty without `script`).
int runWatched(const QStringList &command, QProcessEnvironment env, QStringList *shown)
{
    const QString script = QStandardPaths::findExecutable(QStringLiteral("script"));
    QTemporaryDir recording(recordingTemplate());
    if (script.isEmpty() || !recording.isValid() || !stdinIsTerminal()) {
        return runInTerminal(command.first(), command.mid(1), env);
    }
    const QString typescript = recording.filePath(QStringLiteral("typescript"));
    // script runs the command with $SHELL: /bin/sh, and the command gets
    // the user's own back (makepkg and the helper may use it).
    const QString shell = env.value(QStringLiteral("SHELL"));
    env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
    const QString line = (shell.isEmpty() ? QStringLiteral("unset SHELL; exec ") : QStringLiteral("SHELL=%1 exec ").arg(shellJoin({shell}))) + shellJoin(command);
    const int code = runInTerminal(script, {QStringLiteral("-q"), QStringLiteral("-e"), QStringLiteral("-f"), QStringLiteral("-c"), line, typescript}, env, typescript);
    if (shown) {
        QFile recorded(typescript);
        if (recorded.open(QIODevice::ReadOnly)) {
            for (const QString &l : Kyu::terminalLines(recorded.readAll())) {
                if (!l.trimmed().isEmpty() && !l.startsWith(QLatin1String("Script "))) {
                    shown->append(l.trimmed());
                }
            }
        }
    }
    return code;
}

QString shellJoin(const QStringList &args)
{
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_./=:,+-]+$"));
    QStringList quoted;
    for (const QString &a : args) {
        quoted.append(plain.match(a).hasMatch() ? a : QLatin1Char('\'') + QString(a).replace(QLatin1Char('\''), QStringLiteral("'\\''")) + QLatin1Char('\''));
    }
    return quoted.join(QLatin1Char(' '));
}

// pacdiff, as you with sudo for the files (your DIFFPROG stays); with doas
// or pkexec all of it as root. False when it could not start at all.
bool runPacdiff(Out &out, const QString &elevate)
{
    if (QStandardPaths::findExecutable(QStringLiteral("pacdiff")).isEmpty()) {
        out.warn(Kyu::I18n::tr("pacdiff is in pacman-contrib: %1").arg(QStringLiteral("sudo pacman -S pacman-contrib")));
        return false;
    }
    if (elevate.isEmpty()) {
        out.warn(Kyu::I18n::tr("pacdiff needs sudo, doas or pkexec to change files in /etc."));
        return false;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // pacdiff stops at once without its viewer (vim -d): another one, so
    // it works on a Plasma desktop without vim (seen in a real run).
    if (!env.contains(QStringLiteral("DIFFPROG")) && QStandardPaths::findExecutable(QStringLiteral("vim")).isEmpty()) {
        if (!QStandardPaths::findExecutable(QStringLiteral("nvim")).isEmpty()) {
            env.insert(QStringLiteral("DIFFPROG"), QStringLiteral("nvim -d"));
        } else if (!(env.value(QStringLiteral("WAYLAND_DISPLAY")).isEmpty() && env.value(QStringLiteral("DISPLAY")).isEmpty())
                   && !QStandardPaths::findExecutable(QStringLiteral("kompare")).isEmpty()) {
            env.insert(QStringLiteral("DIFFPROG"), QStringLiteral("kompare"));
        } else if (!(env.value(QStringLiteral("WAYLAND_DISPLAY")).isEmpty() && env.value(QStringLiteral("DISPLAY")).isEmpty())
                   && !QStandardPaths::findExecutable(QStringLiteral("meld")).isEmpty()) {
            env.insert(QStringLiteral("DIFFPROG"), QStringLiteral("meld"));
        } else if (!(env.value(QStringLiteral("WAYLAND_DISPLAY")).isEmpty() && env.value(QStringLiteral("DISPLAY")).isEmpty())
                   && !QStandardPaths::findExecutable(QStringLiteral("kdiff3")).isEmpty()) {
            env.insert(QStringLiteral("DIFFPROG"), QStringLiteral("kdiff3"));
        } else {
            env.insert(QStringLiteral("DIFFPROG"), QStringLiteral("diff -u --color=always"));
        }
        out.note(Kyu::I18n::tr("Differences are shown with %1 (set DIFFPROG to change).").arg(env.value(QStringLiteral("DIFFPROG")).section(QLatin1Char(' '), 0, 0)),
                 Tone::Dim);
    }
    //: v, m, r, o and s are pacdiff's own keys: keep them.
    out.note(Kyu::I18n::tr("pacdiff asks for each file: v view the differences, m merge, r remove the new version, "
                           "o use the new version, s skip."),
             Tone::Dim);
    // Watched: its questions count for "The update is waiting for you".
    const int rc = elevate == QLatin1String("sudo") ? runWatched({QStringLiteral("pacdiff"), QStringLiteral("--sudo")}, env)
                                                    : runWatched({elevate, QStringLiteral("pacdiff")}, env);
    if (rc != 0) {
        out.bad(Kyu::I18n::tr("pacdiff stopped (status %1).").arg(rc));
    }
    return true;
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

// The checker after an update, in the background and silent: its messages
// would land in the terminal under Kyu's last line (the tray shows what it
// finds).
void checkInBackground()
{
    QProcess check;
    check.setProgram(checkBinary());
    check.setStandardInputFile(QProcess::nullDevice());
    check.setStandardOutputFile(QProcess::nullDevice());
    check.setStandardErrorFile(QProcess::nullDevice());
    check.startDetached();
}
QList<int> runSession(const QList<QStringList> &commands, const QStringList &headers, bool stopOnFailure, QStringList *lines)
{
    QTemporaryDir recording(recordingTemplate());
    const auto statusFile = [&recording](qsizetype i) {
        return recording.filePath(QStringLiteral("status-%1").arg(i));
    };
    QString shell;
    for (qsizetype i = 0; i < commands.size(); ++i) {
        shell += QStringLiteral("printf %s %1; %2; r=$?; echo $r >%3; ").arg(shellJoin({headers.value(i)}), shellJoin(inhibited(commands.at(i))), shellJoin({statusFile(i)}));
        if (stopOnFailure) {
            shell += QStringLiteral("[ $r -eq 0 ] || exit $r; ");
        }
    }
    shell += QStringLiteral("exit 0");
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
    const QString script = QStandardPaths::findExecutable(QStringLiteral("script"));
    const QString typescript = recording.filePath(QStringLiteral("typescript"));
    if (!script.isEmpty() && recording.isValid() && stdinIsTerminal()) {
        runInTerminal(script, {QStringLiteral("-q"), QStringLiteral("-e"), QStringLiteral("-f"), QStringLiteral("-c"), shell, typescript}, env, typescript);
        QFile ts(typescript);
        if (lines && ts.open(QIODevice::ReadOnly)) {
            for (const QString &l : Kyu::terminalLines(ts.readAll())) {
                if (!l.trimmed().isEmpty() && !l.startsWith(QLatin1String("Script "))) {
                    lines->append(l.trimmed());
                }
            }
        }
    } else {
        runInTerminal(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), shell}, env);
    }
    QList<int> codes;
    for (qsizetype i = 0; i < commands.size(); ++i) {
        QFile status(statusFile(i));
        bool known = false;
        const int code = status.open(QIODevice::ReadOnly) ? status.readAll().trimmed().toInt(&known) : -1;
        codes.append(known ? code : -1);
    }
    return codes;
}
}
