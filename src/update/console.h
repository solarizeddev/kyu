#pragma once

// kyu-update's terminal: its own lines (Out), reading an answer, running a
// command with the terminal (watched for questions), and finding who holds
// pacman's lock. Shared by the update (main.cpp) and --apply (apply.cpp).

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

namespace KyuUpdate
{
// The width of the step names' column ("Packages   sudo pacman -Syu"): the
// longest step name in the user's language, at least 10 (as in English).
int stepWidth();

enum class Tone { Plain, Dim, Info, Good, Warn, Bad };

// Kyu's lines. Basic ANSI colours only, so the terminal's own colour scheme
// draws them, and only on the marks (▸ ✓ ! ✗): bold coloured text turns
// pale or grey in many schemes (Solarized, light ones), so text is bold in
// the scheme's own text colour. Faint (not "bright black", which some
// schemes make the background colour) for the quiet lines. Long lines wrap
// at the terminal's width, under their own text.
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
    // "Kyu · 8 updates: …", once at the top.
    void header(const QString &text)
    {
        print(text, Tone::Info, true);
    }
    // A heading for what follows ("Arch Linux news since your last update").
    void heading(const QString &text)
    {
        print(text, Tone::Plain, true);
    }
    // "▸ Packages   sudo pacman -Syu": what runs next.
    void step(const QString &name, const QString &detail)
    {
        // Blue: green is for "done" only.
        marked(sym("▸", ">"), name.leftJustified(stepWidth()) + QLatin1Char(' ') + detail, Tone::Info);
    }
    // The same, for what happens without Kyu running anything.
    void quietStep(const QString &name, const QString &detail)
    {
        print(sym("▸", ">") + QLatin1Char(' ') + name.leftJustified(stepWidth()) + QLatin1Char(' ') + detail, Tone::Dim, false);
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
        print(QStringLiteral("  ") + text, tone, false, 2);
    }
    // "  Fix: sudo pacman -Sy …": the command bold in the text colour, the
    // one line to read (or copy) whole: never wrapped.
    void fix(const QString &command, const QString &label = QString())
    {
        write(QStringLiteral("  ") + (label.isEmpty() ? Kyu::I18n::tr("Fix") : label) + QStringLiteral(": "), Tone::Plain, false);
        write(command, Tone::Plain, true);
        put(QStringLiteral("\n"));
        flush();
    }
    void blank()
    {
        print(QString(), Tone::Plain, false);
    }
    void prompt(const QString &text)
    {
        write(text, Tone::Plain, true);
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
    // "✓ text": the mark in its colour, the text bold in the text colour.
    void marked(const QString &mark, const QString &text, Tone tone)
    {
        write(mark, tone, false);
        print(QLatin1Char(' ') + text, Tone::Plain, true, int(mark.size()) + 1, int(mark.size()));
    }
    QString sym(const char *utf8, const char *ascii) const
    {
        return QString::fromUtf8(m_utf8 ? utf8 : ascii);
    }
    // indent: the column wrapped lines start at (under the text); used: the
    // columns already written on this line before `text` (a mark).
    void print(const QString &text, Tone tone, bool bold, int indent = 0, int used = 0)
    {
        const QStringList lines = wrapped(text, indent, used);
        for (qsizetype i = 0; i < lines.size(); ++i) {
            if (i > 0) {
                put(QString(indent, QLatin1Char(' ')));
            }
            write(lines.at(i), tone, bold);
            put(QStringLiteral("\n"));
        }
        flush();
    }
    void flush()
    {
        if (!m_capture) {
            std::fflush(stdout);
        }
    }
    // The terminal's width, 0 when unknown (not a terminal).
    static int columns()
    {
        winsize size{};
        return ::isatty(STDOUT_FILENO) && ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 ? size.ws_col : 0;
    }
    // Words to lines that fit the terminal: the first after the `used`
    // columns already on it, keeping its own leading spaces; the others
    // from column `indent`.
    QStringList wrapped(const QString &text, int indent, int used) const
    {
        const int width = m_capture ? 0 : columns();
        if (width <= indent + 20 || used + text.size() <= width) {
            return {text};
        }
        qsizetype lead = 0;
        while (lead < text.size() && text.at(lead) == QLatin1Char(' ')) {
            ++lead;
        }
        QStringList lines;
        QString line = text.left(lead);
        int room = width - used;
        bool empty = true;
        for (const QString &word : text.mid(lead).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            if (!empty && line.size() + 1 + word.size() > room) {
                lines.append(line);
                line.clear();
                room = width - indent;
                empty = true;
            }
            line += (empty ? QString() : QStringLiteral(" ")) + word;
            empty = false;
        }
        lines.append(line);
        return lines;
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
        // Bold: in the scheme's text colour, never coloured.
        if (bold && tone != Tone::Dim) {
            put(QStringLiteral("\x1b[1m%1\x1b[0m").arg(t));
            return;
        }
        const char *colour = tone == Tone::Info ? "34"
            : tone == Tone::Good                ? "32"
            : tone == Tone::Warn                ? "33"
            : tone == Tone::Bad                 ? "31"
            : tone == Tone::Dim                 ? "2"
                                                : "39";
        put(QStringLiteral("\x1b[%1m%2\x1b[0m").arg(QLatin1String(colour), t));
    }
    bool m_color;
    bool m_utf8;
    std::optional<QString> m_capture;
};

bool stdinIsTerminal();
void discardTypedAhead();
extern volatile std::sig_atomic_t answerInterrupted;
void onAnswerInterrupt(int);
extern volatile std::sig_atomic_t hungUp;
void onHangup(int);
QString readAnswer();
// Yes to one of Kyu's [y/N] questions: y, yes, j or ja, in any case.
bool isYes(const QString &answer);
void noop(int);
int runInTerminal(const QString &program, const QStringList &args, const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment(), const QString &watch = QString());
struct Manager {
    QString name;
    int pid = 0;
};
QList<Manager> packageManagers(const QString &procDir);
QString describe(const Manager &m);
QStringList statFields(const QString &procDir, int pid);
qint64 processStart(const QString &procDir, int pid);
qint64 cpuTicks(const QString &procDir, int pid);
QList<Manager> lockHolders(const QString &procDir, const QString &lockPath);
QHash<int, int> parentMap(const QString &procDir);
QList<int> descendants(const QHash<int, int> &parents, int pid);
bool canBeIdle(const Manager &m, const QList<Manager> &all, const QHash<int, int> &parents);
QStringList cpuMarks(const QString &procDir, const QList<Manager> &managers, const QHash<int, int> &parents);
QString unlockCommand(const QString &elevate, const QString &lockPath);
bool canSeeAllProcesses(const QString &procDir);
extern volatile std::sig_atomic_t interrupted;
void onInterrupt(int);
QByteArray readFrom(const QString &path, qint64 offset);
QStringList wrap(const QString &text, int width = 64);
QSet<int> ancestors();
QString bootId();
QJsonObject readJson(const QString &path);
QStringList strings(const QJsonValue &value);
QString snapshotHook(const QStringList &dirs);
QString duration(qint64 ms);
QStringList inhibited(const QStringList &command);
QString recordingTemplate();
int runWatched(const QStringList &command, QProcessEnvironment env = QProcessEnvironment::systemEnvironment(), QStringList *shown = nullptr);
QString shellJoin(const QStringList &args);
bool runPacdiff(Out &out, const QString &elevate);
QString checkBinary();
void checkInBackground();
}
