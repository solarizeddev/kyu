#include "flatpakfollower.h"

#include "paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>

#include <cerrno>
#include <csignal>

namespace
{
bool alive(qint64 pid)
{
    return pid > 0 && (::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM);
}

QJsonObject firstRecord(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readLine()).object();
}
}

FlatpakFollower::FlatpakFollower(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &FlatpakFollower::poll);
}

QString FlatpakFollower::logDir()
{
    return Kyu::Paths::stateDir() + QStringLiteral("/flatpak");
}

QString FlatpakFollower::lockPath()
{
    return Kyu::Paths::stateDir() + QStringLiteral("/flatpak.lock");
}

qint64 FlatpakFollower::runningPid()
{
    // Read the lock without taking it.
    QLockFile lock(lockPath());
    qint64 pid = 0;
    QString host;
    QString app;
    if (!lock.getLockInfo(&pid, &host, &app) || !alive(pid)) {
        return 0;
    }
    return pid;
}

bool FlatpakFollower::attach()
{
    const qint64 pid = runningPid();
    if (!pid) {
        return false;
    }
    // Its log is the newest one whose "started" record names that pid.
    QStringList logs = QDir(logDir()).entryList({QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
    for (auto it = logs.crbegin(); it != logs.crend(); ++it) {
        const QString path = logDir() + QLatin1Char('/') + *it;
        if (firstRecord(path).value(QStringLiteral("pid")).toInteger() == pid) {
            m_pid = pid;
            follow(path);
            return true;
        }
    }
    return false;
}

void FlatpakFollower::start(const QString &afterTx, bool sessionBus)
{
    QStringList args;
    if (!afterTx.isEmpty()) {
        args << QStringLiteral("--after") << afterTx;
    }
    if (sessionBus) {
        args << QStringLiteral("--session-bus");
    }
    const QString sibling = QCoreApplication::applicationDirPath() + QStringLiteral("/kyu-flatpak");
    const QString program = QFileInfo(sibling).isExecutable() ? sibling : QStringLiteral("kyu-flatpak");
    qint64 pid = 0;
    if (!QProcess::startDetached(program, args, {}, &pid)) {
        Q_EMIT startFailed(QStringLiteral("Could not start %1").arg(program));
        return;
    }
    m_pid = pid;
    m_path.clear();
    m_finished = false;
    m_idlePolls = 0;
    m_timer.start(); // poll() finds the log once kyu-flatpak creates it
}

void FlatpakFollower::follow(const QString &path)
{
    m_path = path;
    m_offset = 0;
    m_finished = false;
    m_idlePolls = 0;
    m_timer.start();
    poll();
}

void FlatpakFollower::poll()
{
    if (m_path.isEmpty()) {
        const QStringList logs = QDir(logDir()).entryList({QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
        for (auto it = logs.crbegin(); it != logs.crend(); ++it) {
            const QString path = logDir() + QLatin1Char('/') + *it;
            if (firstRecord(path).value(QStringLiteral("pid")).toInteger() == m_pid) {
                follow(path);
                return;
            }
        }
        if (!alive(m_pid) && ++m_idlePolls > 5) {
            m_timer.stop();
            Q_EMIT startFailed(QStringLiteral("kyu-flatpak exited before writing its log"));
        }
        return;
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(m_offset)) {
        return;
    }
    bool read = false;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        if (!line.endsWith('\n')) {
            break; // being written; read it next time
        }
        m_offset += line.size();
        read = true;
        const QJsonObject r = QJsonDocument::fromJson(line).object();
        if (r.isEmpty()) {
            continue;
        }
        if (r.value(QStringLiteral("kind")).toString() == QLatin1String("finished")) {
            m_finished = true;
        }
        Q_EMIT record(r);
        if (m_finished) {
            m_timer.stop();
            return;
        }
    }
    // Gone without a result: report it once the log has stopped growing.
    if (!read && !alive(m_pid) && ++m_idlePolls > 2) {
        m_timer.stop();
        Q_EMIT runnerLost(m_path);
    } else if (read) {
        m_idlePolls = 0;
    }
}
