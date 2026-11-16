// kyu-update: runs a system update through kyu-helper from a terminal, or
// reattaches to one that is running. A stand-in for the update panel until
// milestone 3, and the tool for testing that an update survives its UI.

#include "helperclient.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

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
        err() << "kyu: answered \"" << r.value(QStringLiteral("question")).toString() << "\" with "
              << r.value(QStringLiteral("answer")).toString() << " (" << r.value(QStringLiteral("reason")).toString() << ")" << Qt::endl;
    } else if (kind == QLatin1String("question")) {
        err() << "kyu: pacman asked \"" << r.value(QStringLiteral("text")).toString() << "\"; stopping it" << Qt::endl;
    } else if (kind == QLatin1String("finished")) {
        err() << "kyu: " << r.value(QStringLiteral("summary")).toString() << Qt::endl;
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
    QCommandLineOption session(QStringLiteral("session"), QStringLiteral("Use a helper on the session bus (development)."));
    parser.addOptions({attach, state, quiet, session});
    parser.process(app);

    HelperClient client(parser.isSet(session) ? HelperClient::Bus::Session : HelperClient::Bus::System);
    const bool beQuiet = parser.isSet(quiet);

    QString error;
    if (!client.subscribe(&error)) {
        err() << "kyu-update: cannot connect to the bus: " << error << Qt::endl;
        return NoHelper;
    }
    QObject::connect(&client, &HelperClient::record, &app, [&](const QJsonObject &r) {
        printRecord(r, beQuiet);
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
