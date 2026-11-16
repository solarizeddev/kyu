// kyu-update: runs a system update through kyu-helper from a terminal, or
// reattaches to one that is running. A stand-in for the update panel until
// milestone 3, and the tool for testing that an update survives its UI.

#include "failuretext.h"
#include "helperclient.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>

#include <unistd.h>

namespace
{
enum ExitCode {
    Success = 0,
    UpdateFailed = 1,
    NoHelper = 2,
    Refused = 3, // not authorized, busy or locked
    HelperLost = 4, // the helper went away mid-update
};

QTextStream &out()
{
    static QTextStream s(stdout);
    return s;
}

QTextStream &err()
{
    static QTextStream s(stderr);
    return s;
}

void printRecord(const QJsonObject &r, bool quiet)
{
    const QString kind = r.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("started")) {
        QStringList command;
        for (const auto &v : r.value(QStringLiteral("command")).toArray()) {
            command.append(v.toString());
        }
        err() << "kyu: running " << command.join(QLatin1Char(' ')) << " (transaction " << r.value(QStringLiteral("tx")).toString();
        if (r.contains(QStringLiteral("pid"))) {
            err() << ", pid " << r.value(QStringLiteral("pid")).toInteger();
        }
        err() << ")" << Qt::endl;
    } else if (kind == QLatin1String("output")) {
        if (!quiet) {
            auto &stream = r.value(QStringLiteral("stream")).toString() == QLatin1String("stderr") ? err() : out();
            stream << r.value(QStringLiteral("text")).toString() << Qt::endl;
        }
    } else if (kind == QLatin1String("progress")) {
        if (quiet) {
            out() << "[" << r.value(QStringLiteral("current")).toInteger() << "/" << r.value(QStringLiteral("total")).toInteger() << "] "
                  << r.value(QStringLiteral("action")).toString() << " " << r.value(QStringLiteral("package")).toString() << Qt::endl;
        }
    } else if (kind == QLatin1String("answered")) {
        const QString why = r.contains(QStringLiteral("reason")) ? r.value(QStringLiteral("reason")).toString()
                                                                 : QStringLiteral("answered by uid %1").arg(r.value(QStringLiteral("uid")).toInteger());
        err() << "kyu: answered \"" << r.value(QStringLiteral("question")).toString() << "\" with " << r.value(QStringLiteral("answer")).toString()
              << " (" << why << ")" << Qt::endl;
    } else if (kind == QLatin1String("question")) {
        if (!r.value(QStringLiteral("handled")).toBool()) {
            err() << "kyu: pacman asked \"" << r.value(QStringLiteral("text")).toString() << "\", which Kyu does not recognise; stopping it"
                  << Qt::endl;
            return;
        }
        // pacman's own text, then what can be typed.
        for (const auto &line : r.value(QStringLiteral("context")).toArray()) {
            err() << line.toString() << Qt::endl;
        }
        err() << r.value(QStringLiteral("text")).toString() << Qt::endl;
        QStringList values;
        for (const auto &a : r.value(QStringLiteral("answers")).toArray()) {
            values.append(a.toObject().value(QStringLiteral("value")).toString());
        }
        err() << "kyu: type " << values.join(QLatin1Char('/')) << " (Enter: pacman's default, "
              << r.value(QStringLiteral("default")).toString() << "), or c to cancel the update. "
              << "The panel can answer too." << Qt::endl;
    } else if (kind == QLatin1String("cancelled")) {
        err() << "kyu: cancelled; stopping pacman" << Qt::endl;
    } else if (kind == QLatin1String("timeout")) {
        err() << "kyu: no answer within " << r.value(QStringLiteral("seconds")).toInteger() << " s; stopping pacman" << Qt::endl;
    } else if (kind == QLatin1String("finished")) {
        err() << "kyu: " << r.value(QStringLiteral("summary")).toString() << Qt::endl;
        const QJsonObject restart = r.value(QStringLiteral("restart")).toObject();
        const QJsonObject kernel = restart.value(QStringLiteral("kernel")).toObject();
        if (kernel.value(QStringLiteral("replaced")).toBool()) {
            err() << "kyu: restart required: running kernel " << kernel.value(QStringLiteral("running")).toString() << ", installed "
                  << (kernel.value(QStringLiteral("installed")).toString().isEmpty() ? QStringLiteral("unknown")
                                                                                     : kernel.value(QStringLiteral("installed")).toString())
                  << Qt::endl;
        }
        if (restart.value(QStringLiteral("systemd")).toBool()) {
            err() << "kyu: restart required: systemd (PID 1) uses replaced libraries" << Qt::endl;
        }
        if (const auto count = restart.value(QStringLiteral("processCount")).toInteger(); count > 0) {
            QStringList names;
            for (const auto &p : restart.value(QStringLiteral("processes")).toArray()) {
                names.append(p.toObject().value(QStringLiteral("name")).toString());
            }
            names.removeDuplicates();
            err() << "kyu: " << count << (count == 1 ? " program uses" : " programs use") << " replaced libraries: " << names.join(QStringLiteral(", "))
                  << Qt::endl;
        }
        for (const auto &c : r.value(QStringLiteral("configs")).toArray()) {
            const QJsonObject o = c.toObject();
            err() << "kyu: " << o.value(QStringLiteral("saved")).toString()
                  << (o.value(QStringLiteral("kind")).toString() == QLatin1String("pacnew") ? " has new default settings for " : " keeps the settings of removed ")
                  << o.value(QStringLiteral("path")).toString() << Qt::endl;
        }
        if (!r.value(QStringLiteral("configs")).toArray().isEmpty()) {
            err() << "kyu: to compare and merge: sudo pacdiff" << Qt::endl;
        }
        const FailureText why = describeFailure(r.value(QStringLiteral("failure")).toObject());
        if (!why.title.isEmpty()) {
            err() << "kyu: " << why.title << ". " << why.text << Qt::endl;
            for (const QString &item : why.items) {
                err() << "kyu:   " << item << Qt::endl;
            }
            if (!why.command.isEmpty()) {
                err() << "kyu: command: " << why.command << Qt::endl;
            }
        }
    }
}

void printPrevious(const QJsonObject &state)
{
    const QJsonObject previous = state.value(QStringLiteral("previous")).toObject();
    if (previous.isEmpty()) {
        out() << "No update is running." << Qt::endl;
        return;
    }
    const QString log = previous.value(QStringLiteral("log")).toString();
    if (previous.value(QStringLiteral("interrupted")).toBool()) {
        out() << "No update is running. The last one (" << previous.value(QStringLiteral("tx")).toString()
              << ") was interrupted: its log ends without a result." << Qt::endl;
    } else {
        out() << "No update is running. Last update: " << previous.value(QStringLiteral("summary")).toString() << Qt::endl;
    }
    out() << "Log: " << log << Qt::endl;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kyu-update"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Updates the system with pacman -Syu through kyu-helper. Closing this program does not stop the update; "
        "run it again with --attach to follow it."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption attach(QStringLiteral("attach"), QStringLiteral("Follow the running update instead of starting one."));
    QCommandLineOption state(QStringLiteral("state"), QStringLiteral("Print the helper's state as JSON and exit."));
    QCommandLineOption quiet(QStringLiteral("quiet"), QStringLiteral("Show progress lines instead of pacman's output."));
    QCommandLineOption session(QStringList{QStringLiteral("session"), QStringLiteral("session-bus")},
                               QStringLiteral("Use a helper on the session bus (development)."));
    QCommandLineOption removeLock(QStringLiteral("remove-stale-lock"),
                                  QStringLiteral("Remove pacman's db.lck if no running process holds it (left over from an interrupted update)."));
    parser.addOptions({attach, state, quiet, session, removeLock});
    parser.process(app);

    HelperClient client(parser.isSet(session) ? HelperClient::Bus::Session : HelperClient::Bus::System);
    const bool beQuiet = parser.isSet(quiet);

    QString error;
    if (!client.subscribe(&error)) {
        err() << "kyu-update: cannot connect to the bus: " << error << Qt::endl;
        return NoHelper;
    }
    // Answers to pacman's questions come from stdin, one line each.
    qulonglong openQuestion = 0;
    QString defaultAnswer;
    QByteArray typed;
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read);
    input.setEnabled(false);
    bool stdinOpen = true;
    auto answerTyped = [&] {
        qsizetype end;
        while (openQuestion && (end = typed.indexOf('\n')) >= 0) {
            const QString line = QString::fromLocal8Bit(typed.left(end)).trimmed();
            typed.remove(0, end + 1);
            if (line == QLatin1String("c")) {
                client.cancel();
            } else {
                client.answer(openQuestion, line.isEmpty() ? defaultAnswer : line);
            }
        }
    };
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&] {
        char buffer[256];
        const ssize_t n = ::read(STDIN_FILENO, buffer, sizeof buffer);
        if (n <= 0) {
            stdinOpen = false; // EOF: leave it to the panel
            input.setEnabled(false);
            return;
        }
        typed.append(buffer, n);
        answerTyped();
    });
    QObject::connect(&client, &HelperClient::replyFinished, &app, [](const QString &name, const QString &message) {
        if (!name.isEmpty()) {
            err() << "kyu: " << name.section(QLatin1Char('.'), -1) << ": " << message << Qt::endl;
        }
    });

    QObject::connect(&client, &HelperClient::record, &app, [&](const QJsonObject &r) {
        printRecord(r, beQuiet);
        const QString kind = r.value(QStringLiteral("kind")).toString();
        if (kind == QLatin1String("question") && r.value(QStringLiteral("handled")).toBool()) {
            openQuestion = static_cast<qulonglong>(r.value(QStringLiteral("id")).toInteger());
            defaultAnswer = r.value(QStringLiteral("default")).toString();
            input.setEnabled(stdinOpen);
            answerTyped(); // lines typed ahead
        } else if (kind == QLatin1String("answered") || kind == QLatin1String("cancelled") || kind == QLatin1String("timeout")) {
            openQuestion = 0;
            input.setEnabled(false);
        }
        if (r.value(QStringLiteral("kind")).toString() == QLatin1String("finished")) {
            app.exit(r.value(QStringLiteral("success")).toBool() ? Success : UpdateFailed);
        }
    });

    QObject::connect(&client, &HelperClient::helperLost, &app, [&](const QString &log) {
        err() << "kyu: kyu-helper exited before the update finished. pacman may still be running or may have been "
                 "stopped; the log ends without a result: "
              << log << Qt::endl;
        app.exit(HelperLost);
    });

    const auto current = client.state(&error);
    if (!current) {
        err() << "kyu-update: cannot reach kyu-helper: " << error << Qt::endl;
        return NoHelper;
    }
    if (parser.isSet(state)) {
        out() << QJsonDocument(*current).toJson(QJsonDocument::Indented);
        return Success;
    }
    if (parser.isSet(removeLock)) {
        QObject::connect(&client, &HelperClient::lockRemoved, &app, [&](const QString &name, const QString &message) {
            if (name.isEmpty()) {
                out() << "kyu-update: removed " << current->value(QStringLiteral("lock")).toObject().value(QStringLiteral("path")).toString()
                      << Qt::endl;
                app.exit(Success);
            } else {
                err() << "kyu-update: " << name.section(QLatin1Char('.'), -1) << ": " << message << Qt::endl;
                app.exit(name.startsWith(QLatin1String("io.github.solarizeddev.kyu.")) ? Refused : NoHelper);
            }
        });
        client.removeStaleLock();
        return app.exec();
    }

    const QString phase = current->value(QStringLiteral("phase")).toString();
    if (parser.isSet(attach)) {
        if (current->value(QStringLiteral("transaction")).toObject().isEmpty()) {
            printPrevious(*current);
            return Success;
        }
        // Replay inside the event loop: a finished transaction ends the
        // replay with app.exit(), which only works once exec() runs.
        QTimer::singleShot(0, &client, [&client, state = *current] {
            client.follow(state);
        });
        return app.exec();
    }

    if (phase == QLatin1String("running")) {
        err() << "kyu-update: an update is already running; use --attach to follow it" << Qt::endl;
        return Refused;
    }
    QObject::connect(&client, &HelperClient::startFinished, &app, [&](const QString &, const QString &name, const QString &message) {
        if (!name.isEmpty()) {
            err() << "kyu-update: " << name.section(QLatin1Char('.'), -1) << ": " << message << Qt::endl;
            app.exit(name.startsWith(QLatin1String("io.github.solarizeddev.kyu.")) ? Refused : NoHelper);
        }
    });
    client.startUpdate();
    return app.exec();
}
