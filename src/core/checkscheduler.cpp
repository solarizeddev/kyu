#include "checkscheduler.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace
{
constexpr int InternalIntervalMs = 60 * 60 * 1000;
constexpr int FirstInternalCheckMs = 60 * 1000;
}

CheckScheduler::CheckScheduler(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(InternalIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &CheckScheduler::runCheckBinary);
}

void CheckScheduler::start()
{
    if (qEnvironmentVariableIntValue("KYU_NO_SYSTEMD") || QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty()) {
        useInternalTimer();
        return;
    }
    // Starting (not enabling) the timer changes nothing persistent: it runs
    // while the user is logged in and the tray has been started once.
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status == QProcess::NormalExit && exitCode == 0) {
            m_mode = Mode::Systemd;
        } else {
            qInfo("kyu-tray: kyu-check.timer not available (%s), checking from the tray",
                  process->readAllStandardError().trimmed().constData());
            useInternalTimer();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            useInternalTimer();
        }
    });
    process->start(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("start"), QStringLiteral("kyu-check.timer")});
}

void CheckScheduler::checkNow()
{
    if (m_mode == Mode::Systemd) {
        QProcess::startDetached(QStringLiteral("systemctl"),
                                {QStringLiteral("--user"), QStringLiteral("start"), QStringLiteral("--no-block"),
                                 QStringLiteral("kyu-check.service")});
    } else {
        runCheckBinary();
    }
}

void CheckScheduler::useInternalTimer()
{
    m_mode = Mode::Internal;
    m_timer.start();
    QTimer::singleShot(FirstInternalCheckMs, this, &CheckScheduler::runCheckBinary);
}

void CheckScheduler::runCheckBinary()
{
    // Detached: the check finishes and writes its state even if the tray exits.
    // kyu-check's lock file makes overlapping starts harmless.
    if (!QProcess::startDetached(checkBinary(), {})) {
        qWarning("kyu-tray: could not start %s", qPrintable(checkBinary()));
    }
}

QString CheckScheduler::checkBinary()
{
    // Testing: a stand-in checker.
    const QString override = qEnvironmentVariable("KYU_CHECK");
    if (!override.isEmpty()) {
        return override;
    }
    const QString sibling = QCoreApplication::applicationDirPath() + QStringLiteral("/kyu-check");
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    if (QFileInfo(QStringLiteral(KYU_CHECK_BINARY)).isExecutable()) {
        return QStringLiteral(KYU_CHECK_BINARY);
    }
    return QStringLiteral("kyu-check");
}
