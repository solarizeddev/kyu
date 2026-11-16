#include "panelcontroller.h"

#include "checkscheduler.h"
#include "helperclient.h"
#include "statewatcher.h"
#include "statustext.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>

namespace
{
QString stageText(const QString &stage)
{
    if (stage == QLatin1String("sync")) {
        return QStringLiteral("Synchronizing package databases");
    }
    if (stage == QLatin1String("resolve")) {
        return QStringLiteral("Resolving dependencies");
    }
    if (stage == QLatin1String("download")) {
        return QStringLiteral("Downloading packages");
    }
    if (stage == QLatin1String("check")) {
        return QStringLiteral("Checking packages");
    }
    if (stage == QLatin1String("commit")) {
        return QStringLiteral("Changing packages");
    }
    if (stage == QLatin1String("hooks")) {
        return QStringLiteral("Running pacman hooks");
    }
    return QStringLiteral("Starting pacman");
}

QString duration(qint64 seconds)
{
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString siblingBinary(const QString &name)
{
    const QString sibling = QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
    return QFileInfo(sibling).isExecutable() ? sibling : name;
}
}

PanelController::PanelController(QObject *parent)
    : QObject(parent)
{
}

void PanelController::setStateWatcher(StateWatcher *watcher)
{
    m_watcher = watcher;
    connect(watcher, &StateWatcher::changed, this, &PanelController::reloadFromWatcher);
    reloadFromWatcher();
}

void PanelController::reloadFromWatcher()
{
    const auto &state = m_watcher->state();
    setUpdateState(state ? *state : Kyu::UpdateState{}, m_watcher->checking());
    m_haveState = state.has_value();
    if (!m_watcher->loadError().isEmpty()) {
        m_message = QStringLiteral("Cannot read the update list: %1").arg(m_watcher->loadError());
    }
    Q_EMIT changed();
}

void PanelController::setHelper(HelperClient *helper, bool sessionBus)
{
    m_helper = helper;
    m_sessionBus = sessionBus;
    connect(helper, &HelperClient::record, this, &PanelController::handleRecord);
    connect(helper, &HelperClient::startFinished, this, &PanelController::handleStartFinished);
    connect(helper, &HelperClient::helperLost, this, &PanelController::handleHelperLost);
}

void PanelController::setCheckScheduler(CheckScheduler *scheduler)
{
    m_scheduler = scheduler;
}

bool PanelController::attach()
{
    if (!m_helper) {
        return false;
    }
    QString error;
    if (!m_helper->subscribe(&error)) {
        m_helperReachable = false;
        m_message = QStringLiteral("Cannot connect to the system bus: %1").arg(error);
        Q_EMIT changed();
        return false;
    }
    const auto state = m_helper->state(&error);
    if (!state) {
        m_helperReachable = false;
        m_message = QStringLiteral("kyu-helper is not available: %1").arg(error);
        Q_EMIT changed();
        return false;
    }
    if (state->value(QStringLiteral("phase")).toString() == QLatin1String("running")) {
        m_helper->follow(*state);
    }
    return true;
}

void PanelController::setUpdateState(const Kyu::UpdateState &state, bool checking)
{
    m_checking = checking;
    m_state = state;
    m_haveState = true;
    if (m_screen == QLatin1String("list")) {
        m_updates.setState(state);
    } else {
        m_listDirty = true;
    }
    Q_EMIT changed();
}

void PanelController::setScreen(const QString &screen)
{
    if (m_screen == screen) {
        return;
    }
    m_screen = screen;
    if (screen == QLatin1String("list") && m_listDirty) {
        m_listDirty = false;
        m_updates.setState(m_state);
    }
    Q_EMIT changed();
}

// --- list -------------------------------------------------------------------

QString PanelController::title() const
{
    if (!m_haveState || (!m_state.pacman.checked && !m_state.flatpak.checked)) {
        return m_checking ? QStringLiteral("Checking for updates") : QStringLiteral("Not checked yet");
    }
    return Kyu::updatesText(m_state.total());
}

QString PanelController::details() const
{
    QStringList parts;
    const qint64 size = m_state.totalSize();
    if (size > 0) {
        parts.append(QLocale().formattedDataSize(size, 1, QLocale::DataSizeIecFormat));
    }
    if (m_checking) {
        parts.append(QStringLiteral("checking now"));
    } else if (m_state.checkedAt.isValid()) {
        parts.append(QStringLiteral("checked %1").arg(Kyu::shortWhen(m_state.checkedAt, QDateTime::currentDateTime())));
    }
    return parts.join(QStringLiteral(" · "));
}

QStringList PanelController::checkProblems() const
{
    return Kyu::failureLines(m_state, QDateTime::currentDateTime());
}

QString PanelController::flatpakNote() const
{
    if (m_updates.flatpakCount() == 0) {
        return {};
    }
    return QStringLiteral("Update all runs pacman -Syu. Flatpaks are listed but not updated yet.");
}

bool PanelController::canUpdate() const
{
    return m_screen == QLatin1String("list") && !m_starting && m_helperReachable && m_updates.pacmanCount() > 0;
}

void PanelController::updateAll()
{
    if (!canUpdate() || !m_helper) {
        return;
    }
    m_starting = true;
    m_message.clear();
    Q_EMIT changed();
    m_helper->startUpdate();
}

void PanelController::handleStartFinished(const QString &, const QString &errorName, const QString &errorMessage)
{
    m_starting = false;
    if (!errorName.isEmpty()) {
        const QString kind = errorName.section(QLatin1Char('.'), -1);
        if (kind == QLatin1String("Busy") && m_helper) {
            // Someone else started one: follow it.
            QString error;
            if (auto state = m_helper->state(&error)) {
                m_helper->follow(*state);
            }
        } else if (kind == QLatin1String("NotAuthorized")) {
            m_message = QStringLiteral("Not authorized: %1").arg(errorMessage);
        } else if (kind == QLatin1String("Locked")) {
            m_message = QStringLiteral("pacman's database is locked: %1").arg(errorMessage);
        } else {
            m_message = QStringLiteral("Could not start the update: %1").arg(errorMessage);
        }
    }
    Q_EMIT changed();
}

void PanelController::checkAgain()
{
    if (m_scheduler) {
        m_scheduler->checkNow();
    }
}

void PanelController::copyCommand()
{
    // What the user would run themselves to do the same.
    QGuiApplication::clipboard()->setText(QStringLiteral("sudo pacman -Syu"));
}

// --- transaction ------------------------------------------------------------

void PanelController::handleRecord(const QJsonObject &r)
{
    const QString kind = r.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("started")) {
        m_tx = r.value(QStringLiteral("tx")).toString();
        QStringList command;
        for (const auto &v : r.value(QStringLiteral("command")).toArray()) {
            command.append(v.toString());
        }
        m_commandLine = command.join(QLatin1Char(' '));
        m_pid = r.value(QStringLiteral("pid")).toInteger();
        m_uid = r.value(QStringLiteral("uid")).toInteger(-1);
        m_startedAt = QDateTime::fromString(r.value(QStringLiteral("time")).toString(), Qt::ISODate);
        m_finishedAt = {};
        m_logPath = m_helper ? m_helper->logPath() : QString();
        m_stage.clear();
        m_action.clear();
        m_current = 0;
        m_total = 0;
        m_question.clear();
        m_summary.clear();
        m_haveExitCode = false;
        m_message.clear();
        m_output.clear();
        m_output.append(QStringLiteral("$ ") + m_commandLine, QStringLiteral("kyu"));
        if (m_screen == QLatin1String("finished")) {
            m_updates.setState(m_state);
        }
        m_updates.beginTransaction();
        setScreen(QStringLiteral("running"));
    } else if (kind == QLatin1String("output")) {
        m_output.append(r.value(QStringLiteral("text")).toString(), r.value(QStringLiteral("stream")).toString());
        return; // no property changed
    } else if (kind == QLatin1String("stage")) {
        m_stage = r.value(QStringLiteral("stage")).toString();
    } else if (kind == QLatin1String("packages")) {
        m_total = r.value(QStringLiteral("total")).toInt();
    } else if (kind == QLatin1String("progress")) {
        m_current = r.value(QStringLiteral("current")).toInt();
        m_total = qMax(m_total, r.value(QStringLiteral("total")).toInt());
        m_action = r.value(QStringLiteral("action")).toString();
        m_updates.applyProgress(m_action, r.value(QStringLiteral("package")).toString());
    } else if (kind == QLatin1String("answered")) {
        m_output.append(QStringLiteral("Kyu answered \"%1\" with %2 (%3)")
                            .arg(r.value(QStringLiteral("question")).toString(),
                                 r.value(QStringLiteral("answer")).toString(),
                                 r.value(QStringLiteral("reason")).toString()),
                        QStringLiteral("kyu"));
    } else if (kind == QLatin1String("question")) {
        m_question = r.value(QStringLiteral("text")).toString();
    } else if (kind == QLatin1String("finished")) {
        m_success = r.value(QStringLiteral("success")).toBool();
        m_summary = r.value(QStringLiteral("summary")).toString();
        m_haveExitCode = r.contains(QStringLiteral("exitCode")) && !r.value(QStringLiteral("exitCode")).isNull();
        m_exitCode = r.value(QStringLiteral("exitCode")).toInt();
        m_finishedAt = QDateTime::fromString(r.value(QStringLiteral("time")).toString(), Qt::ISODate);
        m_updates.finishTransaction(m_success);
        setScreen(QStringLiteral("finished"));
        // The tray and the next list should reflect the new system state.
        checkAgain();
    } else {
        return;
    }
    Q_EMIT changed();
}

void PanelController::handleHelperLost(const QString &logPath)
{
    m_success = false;
    m_summary = QStringLiteral("kyu-helper stopped before the update finished. pacman may still be running or may have been "
                               "stopped. The log ends without a result: %1")
                    .arg(logPath);
    m_haveExitCode = false;
    m_finishedAt = QDateTime::currentDateTimeUtc();
    m_updates.finishTransaction(false);
    setScreen(QStringLiteral("finished"));
    Q_EMIT changed();
}

int PanelController::percent() const
{
    // Share of package operations completed: the one pacman is working on
    // does not count until the next one starts (or the hooks run).
    if (m_screen == QLatin1String("finished") || m_stage == QLatin1String("hooks")) {
        return 100;
    }
    if (m_total <= 0) {
        return 0;
    }
    return qBound(0, qMax(0, m_current - 1) * 100 / m_total, 100);
}

QString PanelController::progressText() const
{
    if (m_current > 0 && m_stage != QLatin1String("hooks")) {
        return QStringLiteral("%1 %2 of %3").arg(m_action).arg(m_current).arg(m_total);
    }
    return stageText(m_stage);
}

QString PanelController::processLine() const
{
    QStringList parts;
    if (m_pid > 0) {
        parts.append(QStringLiteral("pacman pid %1").arg(m_pid));
    }
    parts.append(m_sessionBus ? QStringLiteral("development helper on the session bus, not root")
                              : QStringLiteral("running as root via polkit"));
    if (m_uid >= 0) {
        parts.append(QStringLiteral("started by uid %1").arg(m_uid));
    }
    return parts.join(QStringLiteral(" · "));
}

void PanelController::copyLog()
{
    QGuiApplication::clipboard()->setText(m_output.text());
}

void PanelController::followInTerminal()
{
    // kyu-update --attach prints the same output live, from the same log.
    QStringList args{QStringLiteral("-e"), siblingBinary(QStringLiteral("kyu-update")), QStringLiteral("--attach")};
    if (m_sessionBus) {
        args.append(QStringLiteral("--session-bus"));
    }
    if (QStandardPaths::findExecutable(QStringLiteral("konsole")).isEmpty() || !QProcess::startDetached(QStringLiteral("konsole"), args)) {
        m_message = QStringLiteral("Could not start Konsole. In a terminal, run: kyu-update --attach");
        Q_EMIT changed();
    }
}

// --- finished ---------------------------------------------------------------

QString PanelController::resultTitle() const
{
    return m_success ? m_summary : QStringLiteral("Update failed");
}

QString PanelController::resultText() const
{
    return m_success ? QString() : m_summary;
}

QString PanelController::resultDetails() const
{
    QStringList parts;
    if (m_finishedAt.isValid()) {
        parts.append(QStringLiteral("finished %1").arg(Kyu::shortWhen(m_finishedAt, QDateTime::currentDateTime())));
    }
    if (m_startedAt.isValid() && m_finishedAt.isValid()) {
        parts.append(QStringLiteral("took %1").arg(duration(m_startedAt.secsTo(m_finishedAt))));
    }
    if (m_haveExitCode) {
        parts.append(QStringLiteral("pacman exit code %1").arg(m_exitCode));
    }
    return parts.join(QStringLiteral(" · "));
}

QVariantList PanelController::changes() const
{
    QVariantList list;
    for (int row : m_updates.changedRows()) {
        const QModelIndex i = m_updates.index(row);
        list.append(QVariantMap{
            {QStringLiteral("name"), i.data(UpdatesModel::NameRole)},
            {QStringLiteral("action"), i.data(UpdatesModel::ActionRole)},
            {QStringLiteral("oldVersion"), i.data(UpdatesModel::OldVersionRole)},
            {QStringLiteral("newVersion"), i.data(UpdatesModel::NewVersionRole)},
        });
    }
    return list;
}

void PanelController::done()
{
    setScreen(QStringLiteral("list"));
}
