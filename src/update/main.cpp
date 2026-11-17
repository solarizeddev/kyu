// kyu-update: the update, in your terminal. Runs the real `sudo pacman -Syu`
// and then `flatpak update`, with their own output and their own questions
// ("Proceed?" included), and ends with one verdict: done, or why it failed,
// and what is left to do, each with one key: [r] restart, [m] merge the
// settings files (pacdiff), [f] fix and update again.
//
// Before pacman: the distro news since the last full upgrade (and a
// "Continue? [y/N]" when there is any), and pacman's lock. Nothing here runs
// as root but pacman itself (through sudo, doas or pkexec) and what a key
// runs.

#include "aftercare.h"
#include "failure.h"
#include "failuretext.h"
#include "keyrings.h"
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
#include <termios.h>
#include <unistd.h>

namespace
{
enum class Tone { Plain, Dim, Info, Good, Warn, Bad };

// Kyu's lines. Basic ANSI colours only, so the terminal's own colour scheme
// draws them; faint (not "bright black", which some schemes make the
// background colour) for the quiet ones.
class Out
{
public:
    explicit Out(bool utf8)
        : m_color(::isatty(STDOUT_FILENO) && qEnvironmentVariableIsEmpty("NO_COLOR"))
        , m_utf8(utf8)
    {
    }
    // The window's title, where the terminal shows it.
    void windowTitle()
    {
        if (m_color) {
            std::fputs("\x1b]0;Kyu\x07", stdout);
        }
    }
    // On a light background yellow and green text is hard to read (bold
    // makes it the scheme's pale "intense" colour): only the marks get colour.
    void setLightBackground(bool light)
    {
        m_light = light;
    }
    // "Kyu · 8 updates: …", once at the top.
    void header(const QString &text)
    {
        print(text, Tone::Info, true);
    }
    // A heading for what follows ("Arch Linux news since your last update").
    void heading(const QString &text, Tone tone)
    {
        print(text, readable(tone), true);
    }
    // "▸ Packages   sudo pacman -Syu": what runs next.
    void step(const QString &name, const QString &detail)
    {
        // Blue: green is for "done" only.
        marked(sym("▸", ">"), name.leftJustified(10) + QLatin1Char(' ') + detail, Tone::Info);
    }
    // The same, for what happens without Kyu running anything.
    void quietStep(const QString &name, const QString &detail)
    {
        print(sym("▸", ">") + QLatin1Char(' ') + name.leftJustified(10) + QLatin1Char(' ') + detail, Tone::Dim, false);
    }
    void good(const QString &text)
    {
        marked(sym("✓", "ok"), text, Tone::Good);
    }
    void warn(const QString &text)
    {
        marked(QStringLiteral("!"), text, Tone::Warn);
    }
    void bad(const QString &text)
    {
        marked(sym("✗", "x"), text, Tone::Bad);
    }
    // An indented line under one of the above.
    void note(const QString &text, Tone tone = Tone::Plain)
    {
        print(QStringLiteral("  ") + text, tone, false);
    }
    void blank()
    {
        print(QString(), Tone::Plain, false);
    }
    void prompt(const QString &text, Tone tone)
    {
        write(text, readable(tone), true);
        std::fflush(stdout);
    }
    // A blank line and step(), as text for a shell to print later (between
    // two commands that run in one go).
    QString stepText(const QString &name, const QString &detail)
    {
        m_capture.emplace();
        blank();
        step(name, detail);
        QString text = *m_capture;
        m_capture.reset();
        return text;
    }

private:
    Tone readable(Tone tone) const
    {
        return m_light && (tone == Tone::Good || tone == Tone::Warn) ? Tone::Plain : tone;
    }
    // "✓ text": all in the tone's colour, or on a light background the mark
    // in colour and the text in the normal text colour, bold.
    void marked(const QString &mark, const QString &text, Tone tone)
    {
        if (readable(tone) == tone) {
            print(mark + QLatin1Char(' ') + text, tone, true);
            return;
        }
        write(mark, tone, false);
        print(QLatin1Char(' ') + text, Tone::Plain, true);
    }
    QString sym(const char *utf8, const char *ascii) const
    {
        return QString::fromUtf8(m_utf8 ? utf8 : ascii);
    }
    void print(const QString &text, Tone tone, bool bold)
    {
        write(text, tone, bold);
        put(QStringLiteral("\n"));
        if (!m_capture) {
            std::fflush(stdout);
        }
    }
    void put(const QString &text)
    {
        if (m_capture) {
            m_capture->append(text);
        } else {
            std::fputs(text.toLocal8Bit().constData(), stdout);
        }
    }
    void write(const QString &text, Tone tone, bool bold)
    {
        QString t = text;
        if (!m_utf8) {
            t.replace(QStringLiteral("·"), QStringLiteral("-")).replace(QStringLiteral("…"), QStringLiteral("..."));
        }
        if (!m_color || (tone == Tone::Plain && !bold) || t.isEmpty()) {
            put(t);
            return;
        }
        const char *colour = tone == Tone::Info ? "34"
            : tone == Tone::Good                ? "32"
            : tone == Tone::Warn                ? "33"
            : tone == Tone::Bad                 ? "31"
            : tone == Tone::Dim                 ? "2"
                                                : "39";
        put(QStringLiteral("\x1b[%1%2m%3\x1b[0m").arg(QLatin1String(bold && tone != Tone::Dim ? "1;" : ""), QLatin1String(colour), t));
    }
    bool m_color;
    bool m_utf8;
    bool m_light = false;
    std::optional<QString> m_capture;
};

// Asks the terminal for its background colour (OSC 11, answered by Konsole,
// xterm, VTE, kitty, foot and others): true for a light one. A "device
// attributes" query (DA1) follows it: nearly every terminal answers that one,
// in order, so once its answer is in nothing else can arrive late and be
// read as a typed answer. Whatever is queued is dropped on the way out.
bool lightBackground()
{
    if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) {
        return false;
    }
    termios old{};
    if (::tcgetattr(STDIN_FILENO, &old) != 0) {
        return false;
    }
    termios raw = old;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    ::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    std::fputs("\x1b]11;?\x1b\\\x1b[c", stdout);
    std::fflush(stdout);
    QByteArray reply;
    static const QRegularExpression da1(QStringLiteral("\\x1b\\[\\?[0-9;]*c"));
    QElapsedTimer waited;
    waited.start();
    while (waited.elapsed() < 1000 && reply.size() < 256 && !da1.match(QString::fromLatin1(reply)).hasMatch()) {
        pollfd in{STDIN_FILENO, POLLIN, 0};
        if (::poll(&in, 1, int(1000 - waited.elapsed())) <= 0) {
            continue;
        }
        char buffer[64];
        const ssize_t n = ::read(STDIN_FILENO, buffer, sizeof buffer);
        if (n > 0) {
            reply.append(buffer, int(n));
        } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
            break; // the terminal is gone
        }
    }
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
    static const QRegularExpression rgb(QStringLiteral("rgb:([0-9a-fA-F]{1,4})/([0-9a-fA-F]{1,4})/([0-9a-fA-F]{1,4})"));
    const QRegularExpressionMatch m = rgb.match(QString::fromLatin1(reply));
    if (!m.hasMatch()) {
        return false;
    }
    auto channel = [&m](int i) {
        const QString hex = m.captured(i);
        return hex.toInt(nullptr, 16) / double((1 << (4 * hex.size())) - 1);
    };
    return 0.2126 * channel(1) + 0.7152 * channel(2) + 0.0722 * channel(3) > 0.5;
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

// Processes named like a package manager that takes pacman's lock.
struct Manager {
    QString name;
    int pid = 0;
};

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

QString shellJoin(const QStringList &args);

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
    return s < 60 ? QStringLiteral("%1 s").arg(s) : QStringLiteral("%1 min").arg((s + 30) / 60);
}

// The command to run pacman with, kept from sleeping where logind allows it.
QStringList inhibited(const QStringList &command)
{
    const QString inhibit = QStandardPaths::findExecutable(QStringLiteral("systemd-inhibit"));
    if (inhibit.isEmpty()) {
        return command;
    }
    const QStringList args{QStringLiteral("--no-ask-password"), QStringLiteral("--what=idle:sleep:shutdown"), QStringLiteral("--who=Kyu"),
                           QStringLiteral("--why=Updating packages"), QStringLiteral("--mode=block")};
    QProcess probe;
    probe.setStandardInputFile(QProcess::nullDevice());
    probe.start(inhibit, args + QStringList{QStringLiteral("true")});
    if (!probe.waitForFinished(5000) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        return command;
    }
    return QStringList{inhibit} + args + command;
}

// A command line for `script -c` (run by /bin/sh).
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
        out.warn(QStringLiteral("pacdiff is in pacman-contrib: sudo pacman -S pacman-contrib"));
        return false;
    }
    if (elevate.isEmpty()) {
        out.warn(QStringLiteral("pacdiff needs sudo, doas or pkexec to change files in /etc."));
        return false;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains(QStringLiteral("DIFFPROG")) && QStandardPaths::findExecutable(QStringLiteral("vim")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("nvim")).isEmpty()) {
        env.insert(QStringLiteral("DIFFPROG"), QStringLiteral("nvim -d"));
    }
    out.note(QStringLiteral("pacdiff asks for each file: v view the differences, m merge, r remove the new version, "
                            "o use the new version, s skip."),
             Tone::Dim);
    const int rc = elevate == QLatin1String("sudo") ? runInTerminal(QStringLiteral("pacdiff"), {QStringLiteral("--sudo")}, env)
                                                    : runInTerminal(elevate, {QStringLiteral("pacdiff")}, env);
    if (rc != 0) {
        out.bad(QStringLiteral("pacdiff stopped (status %1).").arg(rc));
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
}

int main(int argc, char **argv)
{
    // The terminal's character set, from the environment: asked before Qt
    // starts, which switches a non-UTF-8 locale to C.UTF-8 for itself.
    std::setlocale(LC_ALL, "");
    const bool utf8 = QByteArray(::nl_langinfo(CODESET)) == "UTF-8";
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kyu-update"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Updates the system in this terminal: sudo pacman -Syu, then flatpak update, "
                                                    "with the news before and what is left to do after."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noWait(QStringLiteral("no-wait"), QStringLiteral("Do not wait for a key at the end."));
    QCommandLineOption noCheck(QStringLiteral("no-check"), QStringLiteral("Do not run kyu-check afterwards."));
    QCommandLineOption refresh(QStringLiteral("refresh"), QStringLiteral("Download all package databases again (pacman -Syyu), as after changing mirrors."));
    QCommandLineOption stateFile(QStringLiteral("state-file"), QStringLiteral("kyu-check's state (testing)."), QStringLiteral("path"));
    QCommandLineOption pacmanLog(QStringLiteral("pacman-log"), QStringLiteral("pacman's log (testing)."), QStringLiteral("path"),
                                 QStringLiteral("/var/log/pacman.log"));
    QCommandLineOption dbLock(QStringLiteral("db-lock"), QStringLiteral("pacman's lock file (testing)."), QStringLiteral("path"),
                              QStringLiteral("/var/lib/pacman/db.lck"));
    QCommandLineOption procDir(QStringLiteral("proc-dir"), QStringLiteral("/proc (testing)."), QStringLiteral("path"), QStringLiteral("/proc"));
    QCommandLineOption modulesDir(QStringLiteral("modules-dir"), QStringLiteral("/usr/lib/modules (testing)."), QStringLiteral("path"),
                                  QStringLiteral("/usr/lib/modules"));
    QCommandLineOption mergeOnly(QStringLiteral("merge"), QStringLiteral("Only merge the settings files the last update left (pacdiff)."));
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
    QCommandLineOption hooksDir(QStringLiteral("hooks-dir"), QStringLiteral("pacman's hook directories (testing)."), QStringLiteral("path"));
    parser.addOptions({noWait, noCheck, refresh, mergeOnly, lockWait, idleAfter, newsRead, synced, stateFile, pacmanLog, dbLock, procDir, modulesDir, hooksDir});
    parser.process(app);

    Out out(utf8);
    out.setLightBackground(qEnvironmentVariableIsEmpty("NO_COLOR") && lightBackground());
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
    };

    // The end: the lock goes first (the tray's card stops saying "Updating"),
    // then the last line offers what is left to do.
    // The exit status says what happened (0 done, 1 pacman failed, 3 already
    // running, 4 stopped at the news, 5 flatpak failed) only without the wait
    // (--no-wait, or no terminal): after it the person has read the result,
    // last-run.json has it, and a non-zero status would only make Konsole
    // post a "Program exited with status" notification.
    const QString activityPath = Kyu::Paths::stateDir() + QStringLiteral("/activity");
    auto finish = [&](int code, const Keys &keys = {}) {
        QFile::remove(activityPath);
        lock.unlock();
        if (parser.isSet(noWait) || !stdinIsTerminal()) {
            return code;
        }
        Keys k = keys;
        for (;;) {
            QStringList offer;
            if (!k.fix.isEmpty()) {
                offer.append(QStringLiteral("[f] fix, update again"));
            }
            if (k.restart) {
                offer.append(QStringLiteral("[r] restart"));
            }
            if (!k.configs.isEmpty()) {
                offer.append(QStringLiteral("[m] merge"));
            }
            offer.append(QStringLiteral("[Enter] close"));
            out.blank();
            // Keys pressed while flatpak or pacdiff ran are not answers to this.
            discardTypedAhead();
            out.prompt(offer.join(QStringLiteral("   ")) + QLatin1Char(' '), Tone::Info);
            const QString answer = readAnswer();
            if (answer.isEmpty()) {
                return 0;
            }
            if (answer == QLatin1String("r") && k.restart) {
                // Plasma's own dialog first: programs can save, and it asks.
                const int prompted = QProcess::execute(QStringLiteral("busctl"),
                                                       {QStringLiteral("--user"), QStringLiteral("call"), QStringLiteral("org.kde.LogoutPrompt"),
                                                        QStringLiteral("/LogoutPrompt"), QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("promptReboot")});
                if (prompted == 0) {
                    out.note(QStringLiteral("Plasma asks whether to restart now."), Tone::Dim);
                    continue;
                }
                out.prompt(QStringLiteral("Restart now? Unsaved work in open programs is lost. [y/N] "), Tone::Warn);
                if (readAnswer() != QLatin1String("y")) {
                    continue;
                }
                const int rc = runInTerminal(QStringLiteral("systemctl"), {QStringLiteral("reboot")});
                if (rc == 0) {
                    return 0;
                }
                out.bad(QStringLiteral("systemctl reboot failed (status %1).").arg(rc));
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
                    out.good(QStringLiteral("Settings files merged."));
                }
            } else if (answer == QLatin1String("f") && !k.fix.isEmpty()) {
                if (k.fixAllowed && !k.fixAllowed()) {
                    continue;
                }
                const int rc = runInTerminal(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), k.fix});
                if (rc != 0) {
                    out.bad(QStringLiteral("The fix failed (status %1); its messages are above.").arg(rc));
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
                out.bad(QStringLiteral("Could not start kyu-update again: run it once more yourself."));
            }
        }
    };

    // One update at a time; the tray's card reads this lock too.
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        QString host, appName;
        lock.getLockInfo(&pid, &host, &appName);
        out.warn(QStringLiteral("An update is already running (pid %1).").arg(pid));
        if (parser.isSet(noWait) || !stdinIsTerminal()) {
            return 3;
        }
        out.blank();
        out.prompt(QStringLiteral("[Enter] close "), Tone::Info);
        readAnswer();
        return 0;
    }

    // What this run does, for the tray and the card while the lock is held
    // (a file left by a killed run is replaced here).
    {
        QSaveFile activity(activityPath);
        if (activity.open(QIODevice::WriteOnly)) {
            activity.write(parser.isSet(mergeOnly) ? "merge\n" : "update\n");
            activity.commit();
        }
    }

    // --- Settings files only (the card's Merge…) ----------------------------
    if (parser.isSet(mergeOnly)) {
        out.header(QStringLiteral("Kyu · settings files"));
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
                out.good(QStringLiteral("Settings files merged."));
            } else {
                out.warn(QStringLiteral("%1 still to merge.").arg(plural(left.size(), QStringLiteral("settings file"), QStringLiteral("settings files"))));
            }
        }
        return finish(0);
    }

    // --- Before pacman ----------------------------------------------------
    const QString statePath = parser.isSet(stateFile) ? parser.value(stateFile) : Kyu::Paths::stateFile();
    const std::optional<Kyu::UpdateState> state = Kyu::UpdateState::load(statePath);
    if (!state || !state->checkedAt.isValid() || state->total() == 0) {
        out.header(QStringLiteral("Kyu · updating"));
    } else {
        QStringList parts;
        if (!state->packages.isEmpty()) {
            parts.append(plural(state->packages.size(), QStringLiteral("package"), QStringLiteral("packages")));
        }
        if (!state->flatpaks.isEmpty()) {
            parts.append(plural(state->flatpaks.size(), QStringLiteral("Flatpak"), QStringLiteral("Flatpaks")));
        }
        out.header(QStringLiteral("Kyu · %1: %2").arg(plural(state->total(), QStringLiteral("update"), QStringLiteral("updates")),
                                                     parts.join(QStringLiteral(", "))));
    }
    // Kyu's own window asks before it closes during an update.
    if (qEnvironmentVariableIsEmpty("KYU_WINDOW")) {
        out.note(QStringLiteral("Keep this window open until Kyu says it is done."), Tone::Dim);
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
            out.note(QStringLiteral("News (read): ") + newest.title + QStringLiteral(" · ") + newest.link
                         + (more > 0 ? QStringLiteral(" · +%1 more").arg(more) : QString()),
                     Tone::Dim);
        } else {
            const QList<Kyu::NewsItem> shown = fresh.mid(0, 5);
            out.heading(QStringLiteral("%1 since your last update").arg(state->news.source), Tone::Warn);
            for (const Kyu::NewsItem &item : shown) {
                out.note(item.title);
                out.note(item.link, Tone::Dim);
            }
            if (fresh.size() > shown.size()) {
                out.note(QStringLiteral("… and %1 more: %2").arg(fresh.size() - shown.size()).arg(state->news.page), Tone::Dim);
            }
            // No by default: Enter alone must not skip news that may need action.
            out.prompt(QStringLiteral("Continue? [y/N] "), Tone::Warn);
            const QString answer = readAnswer();
            if (!stdinIsTerminal()) {
                out.blank(); // a typed answer ends the line itself
            }
            if (answer != QLatin1String("y") && answer != QLatin1String("yes")) {
                out.warn(QStringLiteral("Stopped. Nothing was installed."));
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

    // Only Flatpaks pending (by the last check): no sudo, no pacman.
    const bool flatpakOnly = state && state->pacman.checked && state->pacman.ok && state->packages.isEmpty() && !state->flatpaks.isEmpty();
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
        out.warn(QStringLiteral("Waiting for %1 to finish. Ctrl+C stops.").arg(describe(candidates().constFirst())));
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
        Lock result = Lock::TimedOut;
        const bool tty = ::isatty(STDOUT_FILENO);
        while (waited.elapsed() < lockLimit) {
            if (interrupted) {
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
                if (all.isEmpty()) {
                    result = Lock::Stale;
                    break;
                }
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
                std::printf("\r  waited %lld:%02lld ", s / 60, s % 60);
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
            out.warn(QStringLiteral("pacman's lock looks left over: %1 is running but idle.").arg(idleHolder));
        } else if (why == Lock::Unseen) {
            out.warn(QStringLiteral("pacman's lock is there; Kyu cannot see other users' programs to tell who has it."));
        } else {
            out.warn(QStringLiteral("pacman's lock is left over from a stopped run; nothing is using it."));
        }
        Keys keys;
        if (!elevate.isEmpty()) {
            keys.fix = unlockCommand(elevate, lockPath);
            keys.fixShown = QStringLiteral("%1 rm %2, if no program has it open").arg(elevate, lockPath);
            out.note(QStringLiteral("Fix: ") + keys.fixShown, Tone::Info);
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
                        out.warn(QStringLiteral("%1 is working now and may be using the lock: wait until it is done.").arg(describe(m)));
                        return false;
                    }
                }
                const QStringList before = cpuMarks(procPath, all, parents);
                QThread::msleep(1000);
                parents = parentMap(procPath);
                if (cpuMarks(procPath, all, parents) != before) {
                    out.warn(QStringLiteral("%1 is working now and may be using the lock: wait until it is done.").arg(describe(all.constFirst())));
                    return false;
                }
                return true;
            };
        }
        return finish(1, keys);
    };
    if (!flatpakOnly && QFileInfo::exists(lockPath)) {
        out.blank();
        switch (const Lock lock = waitForLock()) {
        case Lock::Free:
            break;
        case Lock::Stopped:
            out.warn(QStringLiteral("Stopped. Nothing was installed."));
            return finish(4);
        case Lock::TimedOut:
            out.warn(QStringLiteral("Still busy after %1. Update when it is done.").arg(duration(lockLimit)));
            return finish(1);
        case Lock::Stale:
        case Lock::Idle:
        case Lock::Unseen:
            return staleLock(lock);
        }
    }

    if (!flatpakOnly && elevate.isEmpty()) {
        out.bad(QStringLiteral("Neither sudo, doas nor pkexec is installed: pacman needs one of them to run as root."));
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
        out.step(steps.first().title, steps.first().command.join(QLatin1Char(' ')));
        for (int attempt = 0; attempt < 2; ++attempt) {
            const qint64 logOffset = QFileInfo(logPath).size();
            // util-linux `script` keeps pacman fully interactive (a terminal of
            // its own) and records what it prints, for the explanation of a
            // failure. The password is never echoed, so never recorded. It runs
            // the command with $SHELL: /bin/sh, whatever the login shell is.
            QTemporaryDir recording;
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
                pacmanExit = runInTerminal(script, {QStringLiteral("-q"), QStringLiteral("-e"), QStringLiteral("-f"), QStringLiteral("-c"), shell, typescript}, env);
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
            failure = pacmanExit != 0 ? Kyu::classifyFailure(pacmanLines) : QJsonObject();
            const bool lockTaken = failure.value(QStringLiteral("kind")).toString() == QLatin1String("lock") && !lastStarted;
            if (attempt > 0 || !lockTaken) {
                break;
            }
            out.blank();
            const Lock lock = waitForLock();
            if (lock == Lock::Stopped) {
                out.warn(QStringLiteral("Stopped. ")
                         + (run.changed.isEmpty() ? QStringLiteral("Nothing was installed.") : QStringLiteral("Only %1 updated.").arg(run.changed.join(QStringLiteral(", ")))));
                return finish(4);
            }
            if (lock == Lock::Stale || lock == Lock::Idle || lock == Lock::Unseen) {
                return staleLock(lock);
            }
            if (lock != Lock::Free) {
                break; // timed out: the verdict says why pacman stopped
            }
            out.step(steps.first().title, steps.first().command.join(QLatin1Char(' ')) + QStringLiteral(" (again)"));
        }
        return std::nullopt;
    };
    if (!flatpakOnly) {
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
            out.quietStep(QStringLiteral("Snapshot"),
                          keyringsFirst.isEmpty() ? QStringLiteral("%1 takes one first").arg(hook)
                                                  : QStringLiteral("%1 takes one before the keyrings and one before the rest").arg(hook));
        }
        const bool refreshAll = parser.isSet(refresh);
        std::optional<int> end;
        if (!keyringsFirst.isEmpty()) {
            end = runPacman({{QStringLiteral("Keyrings"),
                              QStringList{elevate, QStringLiteral("pacman"), refreshAll ? QStringLiteral("-Syy") : QStringLiteral("-Sy"), QStringLiteral("--needed")}
                                  + keyringsFirst},
                             {QStringLiteral("Packages"), {elevate, QStringLiteral("pacman"), QStringLiteral("-Su")}}});
        } else {
            end = runPacman({{QStringLiteral("Packages"), {elevate, QStringLiteral("pacman"), refreshAll ? QStringLiteral("-Syyu") : QStringLiteral("-Syu")}}});
        }
        if (end) {
            return *end;
        }
    }

    // --- flatpak -------------------------------------------------------------
    int flatpakExit = -1;
    if ((flatpakOnly || pacmanExit == 0) && flatpakWanted) {
        out.blank();
        out.step(QStringLiteral("Flatpaks"), QStringLiteral("flatpak update"));
        flatpakExit = runInTerminal(flatpak, {QStringLiteral("update")});
    }

    // --- The verdict ---------------------------------------------------------
    // What is left to do comes from this run and, in the same boot, from
    // earlier ones: the kernel is checked every time.
    const QJsonObject previous = readJson(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
    const QString boot = bootId();
    const bool sameBoot = !boot.isEmpty() && previous.value(QStringLiteral("bootId")).toString() == boot;
    QString restartReason;
    bool restartFromThisRun = false;
    const Kyu::KernelState kernel = Kyu::kernelState(parser.value(procDir), parser.value(modulesDir));
    if (kernel.replaced) {
        restartReason = kernel.installed.isEmpty() ? QStringLiteral("the running kernel %1 was removed").arg(kernel.running)
                                                   : QStringLiteral("new kernel %1").arg(kernel.installed);
        static const QRegularExpression kernelPackage(QStringLiteral("^linux([0-9]+|-lts|-zen|-hardened|-rt.*|-cachyos.*)?$"));
        for (const QString &name : std::as_const(run.updated)) {
            restartFromThisRun = restartFromThisRun || kernelPackage.match(name).hasMatch();
        }
    } else if (const QString pkg = Kyu::restartPackage(run); !pkg.isEmpty()) {
        restartReason = QStringLiteral("%1 was updated").arg(pkg);
        restartFromThisRun = true;
    } else if (sameBoot && previous.value(QStringLiteral("restartRequired")).toBool()) {
        restartReason = previous.value(QStringLiteral("restartReason")).toString();
        if (restartReason.endsWith(QLatin1Char('.'))) {
            restartReason.chop(1); // written by an earlier version
        }
    }

    // Settings files: this run's, and earlier ones still waiting.
    QStringList configs = run.configs;
    for (const QString &path : strings(previous.value(QStringLiteral("configs")))) {
        if (!configs.contains(path) && QFileInfo::exists(path)) {
            configs.append(path);
        }
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
    }

    Keys keys;
    const QString packages = plural(run.changed.size(), QStringLiteral("package"), QStringLiteral("packages"));
    out.blank();
    if (pacmanExit > 0) {
        const QString skipped = flatpakWanted ? QStringLiteral(" Flatpaks were skipped.") : QString();
        // pacman's last lines, whatever the language; the last one is its
        // "::" question when it was answered no.
        QStringList last;
        for (auto it = pacmanLines.crbegin(); it != pacmanLines.crend() && last.size() < 3; ++it) {
            const QString l = it->trimmed();
            if (!l.isEmpty() && !l.startsWith(QLatin1String("Script done"))) {
                last.prepend(l);
            }
        }
        const bool declined = !last.isEmpty() && last.last().startsWith(QLatin1String("::"));
        // What an earlier run of this update (the keyrings) installed.
        const QString only = run.changed.isEmpty()
            ? QStringLiteral("Nothing was installed.")
            : QStringLiteral("Only %1 %2 updated.").arg(run.changed.join(QStringLiteral(", ")), run.changed.size() == 1 ? QStringLiteral("was") : QStringLiteral("were"));
        if (lastStarted) {
            out.bad(QStringLiteral("pacman failed after changing %1.").arg(packages) + skipped);
        } else if (declined) {
            out.warn(QStringLiteral("Stopped. ") + only + skipped);
        } else {
            out.bad((run.changed.isEmpty() ? QString() : QStringLiteral("pacman failed. ")) + only + skipped);
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
        if ((parser.isSet(synced) || listsChanged || keyringsDone || declined || afterSync.contains(kind)) && !lastStarted) {
            out.warn(QStringLiteral("The package lists are newer than the system: update before installing anything."));
        }
        if (!failure.isEmpty()) {
            if (kind == QLatin1String("keyring")) {
                failure.insert(QStringLiteral("keyrings"), QJsonArray::fromStringList(Kyu::installedKeyrings(localDb)));
            }
            FailureText text = describeFailure(failure);
            // Updating the keyrings is what failed, or they are current now:
            // the usual fix does not help.
            if (kind == QLatin1String("keyring") && (keyringsDone || keyringsFailed || parser.isSet(synced))) {
                text.fix.clear();
                const QString keys = QStringLiteral("Check that the clock is right, then reload and refresh pacman's keys: %1 pacman-key --populate, "
                                                    "then %1 pacman-key --refresh-keys (slow). pacman's messages above name the key.")
                                         .arg(elevate.isEmpty() ? QStringLiteral("sudo") : elevate);
                text.text = keyringsFailed ? QStringLiteral("The new keyring itself could not be checked: the installed one is too old to "
                                                            "trust it, which happens after a long gap. ")
                                                 + keys
                                           : QStringLiteral("The keyrings are already current, so a newer keyring is not the answer. ") + keys;
            }
            out.note(text.title + QLatin1Char('.'));
            // Files and package files are not in the sentence: list them.
            if (kind == QLatin1String("files") || kind == QLatin1String("mirror")) {
                for (const QString &item : text.items.mid(0, 3)) {
                    out.note(QStringLiteral("· ") + item, Tone::Dim);
                }
                if (text.items.size() > 3) {
                    out.note(QStringLiteral("· and %1 more").arg(text.items.size() - 3), Tone::Dim);
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
                if (kind == QLatin1String("mirror")) {
                    keys.again.append(QStringLiteral("--refresh"));
                } else if (kind == QLatin1String("keyring")) {
                    keys.again.append(QStringLiteral("--synced"));
                }
                out.note(QStringLiteral("Fix: ") + keys.fix, Tone::Info);
            } else {
                // No key for it: what to do, in words.
                for (const QString &line : wrap(text.text, 72)) {
                    out.note(line, Tone::Dim);
                }
            }
        } else if (!declined && !last.isEmpty()) {
            out.note(QStringLiteral("pacman's last lines (status %1):").arg(pacmanExit));
            for (const QString &l : last) {
                out.note(l, Tone::Dim);
            }
        }
    } else if (flatpakExit > 0) {
        if (!run.changed.isEmpty()) {
            out.good(QStringLiteral("Packages done: %1").arg(packages));
        }
        out.bad(QStringLiteral("Flatpaks failed (status %1), see above.").arg(flatpakExit));
    } else {
        QStringList parts;
        if (!run.changed.isEmpty()) {
            parts.append(packages);
        }
        if (flatpakExit == 0 && state && !state->flatpaks.isEmpty()) {
            parts.append(plural(state->flatpaks.size(), QStringLiteral("Flatpak"), QStringLiteral("Flatpaks")));
        }
        out.good(parts.isEmpty() ? QStringLiteral("Already up to date.")
                                 : QStringLiteral("Done in %1 · %2").arg(duration(clock.elapsed()), parts.join(QStringLiteral(" · "))));
    }

    if (!restartReason.isEmpty()) {
        const bool unfinished = pacmanExit > 0 && lastStarted;
        out.warn(QStringLiteral("Restart needed: ") + restartReason + (restartFromThisRun ? QString() : QStringLiteral(" (from an earlier update)")));
        if (unfinished) {
            // Half an update: the new kernel's boot files may not be there.
            out.note(QStringLiteral("Do not restart until the update has finished."), Tone::Warn);
        } else {
            keys.restart = true;
        }
    }
    if (!configs.isEmpty()) {
        // The file the new version is for: /etc/sddm.conf for /etc/sddm.conf.pacnew.
        QStringList shown;
        for (const QString &path : configs) {
            shown.append(QString(path).remove(QRegularExpression(QStringLiteral("\\.pac(new|save)$"))));
        }
        out.warn(QStringLiteral("%1 changed: %2")
                     .arg(plural(configs.size(), QStringLiteral("settings file"), QStringLiteral("settings files")),
                          shown.mid(0, 3).join(QStringLiteral(", ")) + (shown.size() > 3 ? QStringLiteral(", …") : QString())));
        keys.configs = configs;
    }
    if (!programs.isEmpty()) {
        out.warn(QStringLiteral("Still on old libraries: %1. Restart them.")
                     .arg(programs.mid(0, 6).join(QStringLiteral(", ")) + (programs.size() > 6 ? QStringLiteral(", …") : QString())));
    }
    if (pacmanExit <= 0 && flatpakExit <= 0 && restartReason.isEmpty() && configs.isEmpty() && programs.isEmpty()) {
        out.note(QStringLiteral("Nothing else to do."));
    }

    // For the tray's card: what is left to do.
    QJsonObject record{
        {QStringLiteral("v"), 1},
        {QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("bootId"), boot},
        {QStringLiteral("pacmanExit"), pacmanExit < 0 ? QJsonValue() : QJsonValue(pacmanExit)},
        {QStringLiteral("flatpakExit"), flatpakExit < 0 ? QJsonValue() : QJsonValue(flatpakExit)},
        {QStringLiteral("changed"), int(run.changed.size())},
        {QStringLiteral("transactionStarted"), pacmanExit > 0 ? lastStarted : run.transactionStarted},
        {QStringLiteral("restartRequired"), !restartReason.isEmpty()},
        {QStringLiteral("restartReason"), restartReason},
        {QStringLiteral("configs"), QJsonArray::fromStringList(configs)},
        {QStringLiteral("programs"), QJsonArray::fromStringList(programs)},
        {QStringLiteral("failure"), failure.value(QStringLiteral("kind"))},
    };
    auto save = [&record]() {
        QSaveFile last(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
        if (last.open(QIODevice::WriteOnly)) {
            last.write(QJsonDocument(record).toJson());
            last.commit();
        }
    };
    save();
    // After [m], the card lists only the files still waiting.
    keys.configsLeft = [&record, save](const QStringList &left) {
        record.insert(QStringLiteral("configs"), QJsonArray::fromStringList(left));
        save();
    };

    // The list is stale now: check again (in the background).
    if (!parser.isSet(noCheck)) {
        QProcess::startDetached(checkBinary(), {});
    }

    const int code = pacmanExit > 0 ? pacmanExit : flatpakExit > 0 ? 5 : 0;
    return finish(code, keys);
}
