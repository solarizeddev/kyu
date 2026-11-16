#include "panelcontroller.h"

#include "checkscheduler.h"
#include "failuretext.h"
#include "flatpakfollower.h"
#include "helperclient.h"
#include "statewatcher.h"
#include "statustext.h"

#include <QClipboard>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QFileInfo>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>

#include <unistd.h>

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
    , m_ownUid(::getuid())
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
    connect(helper, &HelperClient::replyFinished, this, &PanelController::handleReplyFinished);
    connect(helper, &HelperClient::lockRemoved, this, [this](const QString &errorName, const QString &errorMessage) {
        m_removingLock = false;
        m_message = errorName.isEmpty() ? QString()
                                        : QStringLiteral("Could not remove the lock: %1").arg(errorMessage);
        refreshHelperState();
        Q_EMIT changed();
    });
}

void PanelController::setCheckScheduler(CheckScheduler *scheduler)
{
    m_scheduler = scheduler;
}

void PanelController::setFlatpakFollower(FlatpakFollower *follower)
{
    m_flatpak = follower;
    connect(follower, &FlatpakFollower::record, this, &PanelController::handleFlatpakRecord);
    connect(follower, &FlatpakFollower::runnerLost, this, &PanelController::handleFlatpakLost);
    connect(follower, &FlatpakFollower::startFailed, this, &PanelController::handleFlatpakStartFailed);
}

bool PanelController::attach()
{
    // A running kyu-flatpak first: if pacman is running too, the run spans
    // both, and the list should show the Flatpaks as part of it.
    if (m_flatpak && m_flatpak->attach()) {
        m_fpRun = true;
    }
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
    setHelperState(*state);
    if (state->value(QStringLiteral("phase")).toString() == QLatin1String("running")) {
        m_helper->follow(*state);
    }
    return true;
}

void PanelController::setHelperState(const QJsonObject &state)
{
    m_previous = state.value(QStringLiteral("previous")).toObject();
    m_lock = state.value(QStringLiteral("lock")).toObject();
    Q_EMIT changed();
}

void PanelController::refreshHelperState()
{
    QString error;
    if (m_helper) {
        if (auto state = m_helper->state(&error)) {
            setHelperState(*state);
        }
    }
}

QString PanelController::interruptedText() const
{
    if (!m_previous.value(QStringLiteral("interrupted")).toBool() || m_inRun) {
        return {};
    }
    return QStringLiteral("The last update (%1) did not finish: its log ends without a result, so pacman may have stopped partway. "
                          "Update all runs pacman -Syu again, which completes it. Log: %2")
        .arg(m_previous.value(QStringLiteral("tx")).toString(), m_previous.value(QStringLiteral("log")).toString());
}

bool PanelController::canRemoveLock() const
{
    return m_lock.value(QStringLiteral("exists")).toBool() && m_lock.value(QStringLiteral("holder")).isNull() && !m_inRun
        && !m_removingLock;
}

void PanelController::removeStaleLock()
{
    if (!canRemoveLock() || !m_helper) {
        return;
    }
    m_removingLock = true;
    Q_EMIT changed();
    m_helper->removeStaleLock();
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
    if (m_state.restartNeeded()) {
        parts.append(QStringLiteral("restart needed"));
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
    if (m_updates.pacmanCount() == 0) {
        return QStringLiteral("Update all updates the Flatpaks (system and user installations).");
    }
    return QStringLiteral("Update all runs pacman -Syu, then updates the Flatpaks.");
}

QString PanelController::newsTitle() const
{
    // News matters before a system upgrade: shown only with pacman updates.
    if (m_updates.pacmanCount() == 0 || (m_state.news.items.isEmpty() && newsError().isEmpty())) {
        return {};
    }
    if (m_state.news.items.isEmpty()) {
        return m_state.news.source;
    }
    const QString since = m_state.news.since.isValid() ? QLocale().toString(m_state.news.since.toLocalTime().date(), QLocale::ShortFormat)
                                                        : QString();
    return since.isEmpty() ? m_state.news.source : QStringLiteral("%1 since your last update (%2)").arg(m_state.news.source, since);
}

QVariantList PanelController::newsItems() const
{
    QVariantList items;
    if (m_updates.pacmanCount() == 0) {
        return items;
    }
    for (const auto &item : m_state.news.items) {
        items.append(QVariantMap{
            {QStringLiteral("title"), item.title},
            {QStringLiteral("link"), item.link},
            {QStringLiteral("date"), QLocale().toString(item.published.toLocalTime().date(), QLocale::ShortFormat)},
        });
    }
    return items;
}

QString PanelController::newsError() const
{
    if (!m_state.news.checked || m_state.news.ok || m_state.news.url.isEmpty()) {
        return {};
    }
    return QStringLiteral("Could not fetch the news: %1").arg(m_state.news.error);
}

void PanelController::openLink(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void PanelController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

bool PanelController::canUpdate() const
{
    if (m_screen != QLatin1String("list") || m_starting) {
        return false;
    }
    if (m_updates.pacmanCount() > 0) {
        return m_helperReachable;
    }
    return m_updates.flatpakCount() > 0 && m_flatpak;
}

void PanelController::updateAll()
{
    if (!canUpdate() || (m_updates.pacmanCount() > 0 && !m_helper)) {
        return;
    }
    m_starting = true;
    m_message.clear();
    m_pacmanRun = false;
    m_pacmanDone = false;
    m_fpRun = m_updates.flatpakCount() > 0 && m_flatpak;
    m_fpDone = false;
    Q_EMIT changed();
    if (m_updates.pacmanCount() > 0) {
        m_helper->startUpdate(); // kyu-flatpak starts once pacman's transaction has an id
    } else {
        m_flatpak->start(QString(), m_sessionBus);
    }
}

void PanelController::handleStartFinished(const QString &transaction, const QString &errorName, const QString &errorMessage)
{
    m_starting = false;
    if (errorName.isEmpty()) {
        // kyu-flatpak waits for this pacman transaction by itself, so the
        // Flatpaks update even if this panel closes in the meantime.
        if (m_fpRun && m_flatpak) {
            m_flatpak->start(transaction, m_sessionBus);
        }
    } else {
        m_fpRun = false;
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
            refreshHelperState();
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
        if (!m_inRun) {
            beginRun();
        }
        m_pacmanRun = true;
        m_pacmanDone = false;
        m_tx = r.value(QStringLiteral("tx")).toString();
        QStringList command;
        for (const auto &v : r.value(QStringLiteral("command")).toArray()) {
            command.append(v.toString());
        }
        m_commandLine = command.join(QLatin1Char(' '));
        m_pid = r.value(QStringLiteral("pid")).toInteger();
        m_uid = r.value(QStringLiteral("uid")).toInteger(-1);
        m_startedAt = QDateTime::fromString(r.value(QStringLiteral("time")).toString(), Qt::ISODate);
        m_logPath = m_helper ? m_helper->logPath() : QString();
        m_stage.clear();
        m_action.clear();
        m_current = 0;
        m_total = 0;
        m_question.clear();
        m_pending = {};
        m_replying = false;
        m_stoppedBy.clear();
        m_summary.clear();
        m_haveExitCode = false;
        m_output.append(QStringLiteral("$ ") + m_commandLine, QStringLiteral("kyu"));
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
    } else if (kind == QLatin1String("answered") && !r.contains(QStringLiteral("id"))) {
        // "Proceed with installation?", answered by the helper itself.
        m_output.append(QStringLiteral("Kyu answered \"%1\" with %2 (%3)")
                            .arg(r.value(QStringLiteral("question")).toString(),
                                 r.value(QStringLiteral("answer")).toString(),
                                 r.value(QStringLiteral("reason")).toString()),
                        QStringLiteral("kyu"));
    } else if (kind == QLatin1String("question")) {
        if (r.value(QStringLiteral("handled")).toBool()) {
            m_pending = r;
            m_replying = false;
            m_message.clear();
            setScreen(QStringLiteral("question"));
        } else {
            m_question = r.value(QStringLiteral("text")).toString();
            m_stoppedBy = QStringLiteral("unsupported");
        }
    } else if (kind == QLatin1String("cancelled") || kind == QLatin1String("timeout")
               || kind == QLatin1String("answered")) {
        if (kind == QLatin1String("answered")) {
            m_output.append(QStringLiteral("Answered \"%1\" with %2 (uid %3)")
                                .arg(r.value(QStringLiteral("question")).toString(),
                                     r.value(QStringLiteral("answer")).toString())
                                .arg(r.value(QStringLiteral("uid")).toInteger()),
                            QStringLiteral("kyu"));
        } else {
            m_stoppedBy = kind;
            m_output.append(kind == QLatin1String("cancelled") ? QStringLiteral("Cancelled; stopping pacman")
                                                               : QStringLiteral("No answer in time; stopping pacman"),
                            QStringLiteral("kyu"));
        }
        m_pending = {};
        m_replying = false;
        setScreen(QStringLiteral("running"));
    } else if (kind == QLatin1String("finished")) {
        m_success = r.value(QStringLiteral("success")).toBool();
        m_summary = r.value(QStringLiteral("summary")).toString();
        m_haveExitCode = r.contains(QStringLiteral("exitCode")) && !r.value(QStringLiteral("exitCode")).isNull();
        m_exitCode = r.value(QStringLiteral("exitCode")).toInt();
        m_finishedAt = QDateTime::fromString(r.value(QStringLiteral("time")).toString(), Qt::ISODate);
        m_pending = {};
        m_replying = false;
        m_restart = r.value(QStringLiteral("restart")).toObject();
        m_failure = r.value(QStringLiteral("failure")).toObject();
        m_configs = r.value(QStringLiteral("configs")).toArray();
        m_pacmanDone = true;
        m_updates.finishKind(QStringLiteral("pacman"), m_success);
        partFinished();
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
    m_pacmanDone = true;
    m_updates.finishKind(QStringLiteral("pacman"), false);
    partFinished();
    Q_EMIT changed();
}

void PanelController::beginRun()
{
    m_inRun = true;
    m_finishedAt = {};
    m_startedAt = {};
    m_message.clear();
    m_output.clear();
    m_stoppedBy.clear();
    m_success = false;
    m_summary.clear();
    m_restart = {};
    m_configs = {};
    m_failure = {};
    m_previous = {};
    m_fpDone = false;
    m_fpSuccess = false;
    m_fpSkipped = false;
    m_fpSummary.clear();
    m_fpStage.clear();
    m_fpCurrent = 0;
    m_fpTotal = 0;
    m_fpLog.clear();
    m_fpPid = 0;
    m_current = 0;
    m_total = 0;
    m_stage.clear();
    if (m_screen == QLatin1String("finished")) {
        m_updates.setState(m_state);
    }
    m_updates.beginTransaction(m_fpRun);
    setScreen(QStringLiteral("running"));
}

void PanelController::partFinished()
{
    const bool pacmanPending = m_pacmanRun && !m_pacmanDone;
    const bool flatpakPending = m_fpRun && !m_fpDone;
    if (!pacmanPending && !flatpakPending) {
        finishRun();
    } else if (m_screen == QLatin1String("question")) {
        setScreen(QStringLiteral("running"));
    }
}

void PanelController::finishRun()
{
    m_inRun = false;
    m_starting = false;
    if (!m_finishedAt.isValid()) {
        m_finishedAt = QDateTime::currentDateTimeUtc();
    }
    m_pending = {};
    m_replying = false;
    setScreen(QStringLiteral("finished"));
    // The tray and the next list should reflect the new system state.
    checkAgain();
}

void PanelController::handleFlatpakRecord(const QJsonObject &r)
{
    const QString kind = r.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("started")) {
        m_fpRun = true;
        m_starting = false;
        if (!m_inRun) {
            beginRun(); // a Flatpak-only run
            m_startedAt = QDateTime::fromString(r.value(QStringLiteral("time")).toString(), Qt::ISODate);
        } else {
            m_updates.includeFlatpak();
        }
        m_fpPid = r.value(QStringLiteral("pid")).toInteger();
        m_fpLog = r.value(QStringLiteral("log")).toString();
        QStringList command;
        for (const auto &v : r.value(QStringLiteral("command")).toArray()) {
            command.append(v.toString());
        }
        m_output.append(QStringLiteral("$ ") + command.join(QLatin1Char(' ')), QStringLiteral("kyu"));
        m_output.append(QStringLiteral("(updates the Flatpaks through libflatpak: the same operations as flatpak update)"), QStringLiteral("kyu"));
    } else if (kind == QLatin1String("output")) {
        m_output.append(r.value(QStringLiteral("text")).toString(), r.value(QStringLiteral("stream")).toString());
        return;
    } else if (kind == QLatin1String("stage")) {
        m_fpStage = r.value(QStringLiteral("stage")).toString();
    } else if (kind == QLatin1String("packages")) {
        m_fpTotal = r.value(QStringLiteral("total")).toInt();
    } else if (kind == QLatin1String("progress")) {
        m_fpCurrent = r.value(QStringLiteral("current")).toInt();
        m_fpTotal = qMax(m_fpTotal, r.value(QStringLiteral("total")).toInt());
        m_fpAction = r.value(QStringLiteral("action")).toString();
        m_updates.applyProgress(m_fpAction, r.value(QStringLiteral("package")).toString(), QStringLiteral("flatpak"));
    } else if (kind == QLatin1String("finished")) {
        m_fpDone = true;
        m_fpSuccess = r.value(QStringLiteral("success")).toBool();
        m_fpSkipped = r.value(QStringLiteral("skipped")).toBool();
        m_fpSummary = r.value(QStringLiteral("summary")).toString();
        m_finishedAt = QDateTime::fromString(r.value(QStringLiteral("time")).toString(), Qt::ISODate);
        m_updates.finishKind(QStringLiteral("flatpak"), m_fpSuccess);
        partFinished();
    } else {
        return;
    }
    Q_EMIT changed();
}

void PanelController::handleFlatpakLost(const QString &logPath)
{
    m_fpDone = true;
    m_fpSuccess = false;
    m_fpSummary = QStringLiteral("kyu-flatpak stopped before it finished. The log ends without a result: %1").arg(logPath);
    m_finishedAt = QDateTime::currentDateTimeUtc();
    m_updates.finishKind(QStringLiteral("flatpak"), false);
    partFinished();
    Q_EMIT changed();
}

void PanelController::handleFlatpakStartFailed(const QString &message)
{
    m_starting = false;
    if (m_inRun) {
        m_fpDone = true;
        m_fpSuccess = false;
        m_fpSummary = message;
        m_updates.finishKind(QStringLiteral("flatpak"), false);
        partFinished();
    } else {
        m_fpRun = false;
        m_message = message;
    }
    Q_EMIT changed();
}

int PanelController::percent() const
{
    // Share of operations completed across both parts: the one in progress
    // does not count until the next one starts.
    if (m_screen == QLatin1String("finished")) {
        return 100;
    }
    int total = 0;
    int done = 0;
    if (m_pacmanRun) {
        const int t = m_total > 0 ? m_total : m_updates.pacmanCount();
        total += t;
        done += (m_pacmanDone || m_stage == QLatin1String("hooks")) ? t : qMax(0, m_current - 1);
    }
    if (m_fpRun) {
        const int t = m_fpTotal > 0 ? m_fpTotal : m_updates.flatpakCount();
        total += t;
        done += m_fpDone ? t : qMax(0, m_fpCurrent - 1);
    }
    return total > 0 ? qBound(0, done * 100 / total, 100) : 0;
}

QString PanelController::progressText() const
{
    if (m_pacmanRun && !m_pacmanDone) {
        QString text = (m_current > 0 && m_stage != QLatin1String("hooks")) ? QStringLiteral("%1 %2 of %3").arg(m_action).arg(m_current).arg(m_total)
                                                                            : stageText(m_stage);
        if (m_fpRun) {
            text += QStringLiteral(" · Flatpaks next");
        }
        return text;
    }
    if (m_fpRun && !m_fpDone) {
        if (m_fpCurrent > 0) {
            return QStringLiteral("%1 Flatpak %2 of %3").arg(m_fpAction).arg(m_fpCurrent).arg(m_fpTotal);
        }
        return m_fpStage == QLatin1String("waiting") ? QStringLiteral("Waiting for pacman") : QStringLiteral("Checking Flatpak remotes");
    }
    return stageText(m_stage);
}

QString PanelController::logPath() const
{
    QStringList paths;
    if (!m_logPath.isEmpty()) {
        paths.append(m_logPath);
    }
    if (!m_fpLog.isEmpty()) {
        paths.append(m_fpLog);
    }
    return paths.join(QStringLiteral(" and "));
}

bool PanelController::success() const
{
    return (!m_pacmanRun || m_success) && (!m_fpRun || m_fpSuccess);
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
    if (m_fpPid > 0) {
        parts.append(QStringLiteral("kyu-flatpak pid %1 in your session").arg(m_fpPid));
    }
    if (!m_pacmanRun) {
        parts.removeAt(m_pid > 0 ? 1 : 0); // no root involved
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

// --- question ---------------------------------------------------------------
//
// Wording is derived from pacman's question only: what it asks, what each
// answer makes pacman do, and pacman's own default. pacman's text is shown
// verbatim next to it.

namespace
{
QString detail(const QJsonObject &q, const char *key)
{
    return q.value(QStringLiteral("details")).toObject().value(QLatin1String(key)).toString();
}
}

QString PanelController::questionType() const
{
    return m_pending.value(QStringLiteral("type")).toString();
}

QString PanelController::questionTitle() const
{
    const QString type = questionType();
    if (type == QLatin1String("provider")) {
        return QStringLiteral("Pick a provider for %1").arg(detail(m_pending, "dependency"));
    }
    if (type == QLatin1String("conflict")) {
        return QStringLiteral("%1 conflicts with %2").arg(detail(m_pending, "install"), detail(m_pending, "remove"));
    }
    if (type == QLatin1String("replace")) {
        return QStringLiteral("Replace %1?").arg(detail(m_pending, "old"));
    }
    if (type == QLatin1String("import_key")) {
        return QStringLiteral("Import a PGP key?");
    }
    if (type == QLatin1String("corrupted")) {
        return QStringLiteral("A downloaded package is corrupted");
    }
    if (type == QLatin1String("skip")) {
        return QStringLiteral("Some packages cannot be upgraded");
    }
    if (type == QLatin1String("ignored")) {
        return QStringLiteral("%1 is in IgnorePkg").arg(detail(m_pending, "package"));
    }
    return QStringLiteral("pacman asks a question");
}

QString PanelController::questionText() const
{
    const QString type = questionType();
    if (type == QLatin1String("provider")) {
        return QStringLiteral("Several packages provide %1. pacman needs one of them.").arg(detail(m_pending, "dependency"));
    }
    if (type == QLatin1String("conflict")) {
        return QStringLiteral("pacman cannot install both.");
    }
    if (type == QLatin1String("replace")) {
        return QStringLiteral("The repos list %1 as the replacement for %2.").arg(detail(m_pending, "new"), detail(m_pending, "old"));
    }
    if (type == QLatin1String("import_key")) {
        return QStringLiteral("pacman needs key %1 (%2) to check package signatures.").arg(detail(m_pending, "key"), detail(m_pending, "uid"));
    }
    if (type == QLatin1String("corrupted")) {
        return QStringLiteral("%1: %2.").arg(detail(m_pending, "file"), detail(m_pending, "reason"));
    }
    if (type == QLatin1String("skip")) {
        return QStringLiteral("pacman offers to skip %1 and upgrade the rest. That would be a partial upgrade, which can break an "
                              "Arch system, so Kyu does not offer it. Cancel the update.")
            .arg(detail(m_pending, "packages"));
    }
    if (type == QLatin1String("ignored")) {
        return QStringLiteral("pacman.conf holds it back with IgnorePkg or IgnoreGroup, but this update needs it.");
    }
    return {};
}

QString PanelController::questionPacman() const
{
    QStringList lines;
    for (const auto &line : m_pending.value(QStringLiteral("context")).toArray()) {
        if (!line.toString().trimmed().isEmpty()) {
            lines.append(line.toString());
        }
    }
    lines.append(m_pending.value(QStringLiteral("text")).toString());
    return lines.join(QLatin1Char('\n'));
}

QString PanelController::questionDefault() const
{
    return m_pending.value(QStringLiteral("default")).toString();
}

QVariantList PanelController::questionOptions() const
{
    const QString type = questionType();
    if (type == QLatin1String("skip")) {
        return {}; // skipping is a partial upgrade: only cancelling is offered
    }
    const QString def = questionDefault();
    auto option = [&def](const QString &value, const QString &label, QString detailText) {
        if (value == def) {
            detailText = detailText.isEmpty() ? QStringLiteral("pacman's default") : detailText + QStringLiteral(" · pacman's default");
        }
        return QVariantMap{{QStringLiteral("value"), value}, {QStringLiteral("label"), label}, {QStringLiteral("detail"), detailText}};
    };
    QVariantList options;
    if (type == QLatin1String("provider")) {
        for (const auto &a : m_pending.value(QStringLiteral("answers")).toArray()) {
            const QJsonObject o = a.toObject();
            options.append(option(o.value(QStringLiteral("value")).toString(),
                                  o.value(QStringLiteral("label")).toString(),
                                  o.value(QStringLiteral("detail")).toString()));
        }
        return options;
    }
    const QString y = QStringLiteral("y");
    const QString n = QStringLiteral("n");
    if (type == QLatin1String("conflict")) {
        const QString install = detail(m_pending, "install");
        const QString remove = detail(m_pending, "remove");
        options = {option(y, QStringLiteral("Remove %1").arg(remove), QStringLiteral("pacman removes %1 and installs %2").arg(remove, install)),
                   option(n, QStringLiteral("Keep %1").arg(remove), QStringLiteral("pacman stops the update; nothing is changed"))};
    } else if (type == QLatin1String("replace")) {
        const QString old = detail(m_pending, "old");
        options = {option(y, QStringLiteral("Replace %1 with %2").arg(old, detail(m_pending, "new")), {}),
                   option(n, QStringLiteral("Keep %1").arg(old), QStringLiteral("pacman continues without the replacement"))};
    } else if (type == QLatin1String("import_key")) {
        options = {option(y, QStringLiteral("Import the key"), QStringLiteral("into pacman's keyring")),
                   option(n, QStringLiteral("Do not import it"), QStringLiteral("packages signed with it fail the check; the update stops"))};
    } else if (type == QLatin1String("corrupted")) {
        options = {option(y, QStringLiteral("Delete the file"), QStringLiteral("the update stops; the next one downloads it again")),
                   option(n, QStringLiteral("Keep the file"), QStringLiteral("the update stops"))};
    } else if (type == QLatin1String("ignored")) {
        const QString package = detail(m_pending, "package");
        options = {option(y, QStringLiteral("Install %1 anyway").arg(package), {}),
                   option(n, QStringLiteral("Leave %1 out").arg(package), {})};
    }
    return options;
}

QString PanelController::questionNote() const
{
    if (!canAnswer()) {
        return QStringLiteral("uid %1 started this update; only that user (or root) can answer.").arg(m_uid);
    }
    const qint64 seconds = m_pending.value(QStringLiteral("timeoutSeconds")).toInteger();
    if (seconds <= 0) {
        return {};
    }
    const QString limit = seconds < 120 ? QStringLiteral("%1 seconds").arg(seconds) : QStringLiteral("%1 minutes").arg((seconds + 59) / 60);
    return QStringLiteral("pacman waits up to %1 for an answer, then Kyu stops it.").arg(limit);
}

QString PanelController::pausedText() const
{
    const int changed = m_updates.changedRows().size() + (m_updates.workingRow() >= 0 ? 1 : 0);
    if (changed == 0) {
        return QStringLiteral("Paused, nothing changed yet");
    }
    return QStringLiteral("Paused, %1 changed so far").arg(changed);
}

bool PanelController::canAnswer() const
{
    return !m_pending.isEmpty() && (m_ownUid == 0 || m_uid < 0 || m_ownUid == m_uid);
}

void PanelController::answer(const QString &value)
{
    if (!canAnswer() || m_replying || !m_helper) {
        return;
    }
    m_replying = true;
    m_message.clear();
    Q_EMIT changed();
    m_helper->answer(static_cast<qulonglong>(m_pending.value(QStringLiteral("id")).toInteger()), value);
}

void PanelController::cancelUpdate()
{
    if (!canAnswer() || m_replying || !m_helper) {
        return;
    }
    m_replying = true;
    m_message.clear();
    Q_EMIT changed();
    m_helper->cancel();
}

void PanelController::handleReplyFinished(const QString &errorName, const QString &errorMessage)
{
    // On success the answered/cancelled record moves the screen on.
    if (!errorName.isEmpty()) {
        m_replying = false;
        m_message = QStringLiteral("%1: %2").arg(errorName.section(QLatin1Char('.'), -1), errorMessage);
        Q_EMIT changed();
    }
}

// --- finished ---------------------------------------------------------------

QString PanelController::resultTitle() const
{
    if (success()) {
        QStringList parts;
        if (m_pacmanRun) {
            parts.append(m_summary);
        }
        if (m_fpRun) {
            parts.append(m_fpSummary);
        }
        QString title = parts.join(QStringLiteral(" · "));
        if (restartRequired()) {
            title += QStringLiteral(", restart required");
        }
        return title;
    }
    if (m_stoppedBy == QLatin1String("cancelled")) {
        return QStringLiteral("Update cancelled");
    }
    if (m_stoppedBy == QLatin1String("timeout")) {
        return QStringLiteral("Update stopped: no answer");
    }
    if (m_stoppedBy == QLatin1String("unsupported")) {
        return QStringLiteral("Update stopped");
    }
    return QStringLiteral("Update failed");
}

QString PanelController::resultText() const
{
    if (success()) {
        return {};
    }
    // Both parts' results, so it is clear what did happen.
    QStringList lines;
    if (m_pacmanRun) {
        lines.append(m_summary);
    }
    if (m_fpRun) {
        lines.append(m_fpSummary);
    }
    return lines.join(QLatin1Char('\n'));
}

QString PanelController::resultDetails() const
{
    QStringList parts;
    const QJsonObject kernel = m_restart.value(QStringLiteral("kernel")).toObject();
    if (kernel.value(QStringLiteral("replaced")).toBool()) {
        parts.append(QStringLiteral("running %1").arg(kernel.value(QStringLiteral("running")).toString()));
        if (!kernel.value(QStringLiteral("installed")).toString().isEmpty()) {
            parts.append(QStringLiteral("installed %1").arg(kernel.value(QStringLiteral("installed")).toString()));
        }
    }
    if (m_finishedAt.isValid()) {
        parts.append(QStringLiteral("finished %1").arg(Kyu::shortWhen(m_finishedAt, QDateTime::currentDateTime())));
    }
    if (m_startedAt.isValid() && m_finishedAt.isValid()) {
        parts.append(QStringLiteral("took %1").arg(duration(m_startedAt.secsTo(m_finishedAt))));
    }
    if (m_pacmanRun && m_haveExitCode) {
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

QString PanelController::failureTitle() const
{
    return describeFailure(m_failure).title;
}

QString PanelController::failureText() const
{
    return describeFailure(m_failure).text;
}

QStringList PanelController::failureItems() const
{
    return describeFailure(m_failure).items;
}

QStringList PanelController::failureLines() const
{
    QStringList lines;
    for (const auto &l : m_failure.value(QStringLiteral("lines")).toArray()) {
        lines.append(l.toString());
    }
    return lines;
}

QString PanelController::failureCommand() const
{
    return describeFailure(m_failure).command;
}

bool PanelController::restartRequired() const
{
    return m_restart.value(QStringLiteral("required")).toBool();
}

QString PanelController::restartReason() const
{
    QStringList reasons;
    const QJsonObject kernel = m_restart.value(QStringLiteral("kernel")).toObject();
    if (kernel.value(QStringLiteral("replaced")).toBool()) {
        reasons.append(QStringLiteral("The running kernel was replaced; new kernel modules load only after a restart."));
    }
    if (m_restart.value(QStringLiteral("systemd")).toBool()) {
        reasons.append(QStringLiteral("systemd (PID 1) still uses replaced libraries."));
    }
    return reasons.join(QLatin1Char(' '));
}

QVariantList PanelController::processes() const
{
    QVariantList list;
    for (const auto &p : m_restart.value(QStringLiteral("processes")).toArray()) {
        const QJsonObject o = p.toObject();
        QStringList libraries;
        for (const auto &l : o.value(QStringLiteral("libraries")).toArray()) {
            libraries.append(l.toString());
        }
        list.append(QVariantMap{{QStringLiteral("pid"), o.value(QStringLiteral("pid")).toInteger()},
                                {QStringLiteral("name"), o.value(QStringLiteral("name")).toString()},
                                {QStringLiteral("libraries"), libraries.join(QLatin1Char('\n'))}});
    }
    return list;
}

int PanelController::processCount() const
{
    return m_restart.value(QStringLiteral("processCount")).toInt();
}

QVariantList PanelController::configs() const
{
    QVariantList list;
    for (const auto &c : m_configs) {
        list.append(c.toObject().toVariantMap());
    }
    return list;
}

QVariantList PanelController::diff(const QString &path, const QString &saved) const
{
    auto error = [](const QString &text) {
        return QVariantList{QVariantMap{{QStringLiteral("text"), text}, {QStringLiteral("kind"), QStringLiteral("error")}}};
    };
    for (const QString &file : {path, saved}) {
        if (!QFileInfo::exists(file)) {
            return error(QStringLiteral("%1 does not exist (any more).").arg(file));
        }
        if (!QFileInfo(file).isReadable()) {
            return error(QStringLiteral("%1 is readable only by root. Compare with: sudo pacdiff").arg(file));
        }
    }
    QProcess process;
    process.start(QStringLiteral("diff"), {QStringLiteral("-u"), path, saved});
    if (!process.waitForFinished(10000) || process.exitCode() > 1) {
        return error(QStringLiteral("diff failed: %1").arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed()));
    }
    QVariantList lines;
    const QStringList output = QString::fromLocal8Bit(process.readAllStandardOutput()).split(QLatin1Char('\n'));
    for (const QString &line : output) {
        QString kind = QStringLiteral("context");
        if (line.startsWith(QLatin1String("---")) || line.startsWith(QLatin1String("+++"))) {
            kind = QStringLiteral("header");
        } else if (line.startsWith(QLatin1String("@@"))) {
            kind = QStringLiteral("hunk");
        } else if (line.startsWith(QLatin1Char('+'))) {
            kind = QStringLiteral("add");
        } else if (line.startsWith(QLatin1Char('-'))) {
            kind = QStringLiteral("remove");
        }
        lines.append(QVariantMap{{QStringLiteral("text"), line}, {QStringLiteral("kind"), kind}});
    }
    if (process.exitCode() == 0) {
        return error(QStringLiteral("The two files are the same."));
    }
    return lines;
}

void PanelController::restartNow()
{
    // Plasma's own prompt first: it lets applications save their work.
    auto session = QDBusConnection::sessionBus();
    if (session.isConnected() && session.interface()->isServiceRegistered(QStringLiteral("org.kde.LogoutPrompt"))) {
        session.call(QDBusMessage::createMethodCall(QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("/LogoutPrompt"),
                                                    QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("promptReboot")),
                     QDBus::NoBlock);
        return;
    }
    // Otherwise logind, which asks polkit if needed.
    auto call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                                               QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("Reboot"));
    call.setArguments({true});
    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        m_message = QStringLiteral("Could not restart: %1").arg(reply.errorMessage());
        Q_EMIT changed();
    }
}

void PanelController::done()
{
    setScreen(QStringLiteral("list"));
}
