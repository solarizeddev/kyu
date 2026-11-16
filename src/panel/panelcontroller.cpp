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
        m_pending = {};
        m_replying = false;
        m_stoppedBy.clear();
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
    if (m_success) {
        return m_summary;
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
