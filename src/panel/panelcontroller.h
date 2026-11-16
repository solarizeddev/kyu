#pragma once

#include "outputmodel.h"
#include "updatesmodel.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

class HelperClient;
class StateWatcher;
class CheckScheduler;

// Everything the panel's QML shows and does. QML only lays it out.
//
// Screens: "list" (pending updates), "running" (a transaction is running,
// started here or elsewhere), "finished" (result of the transaction this
// panel followed). The pacman output view is a page on top of any screen.
class PanelController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString screen READ screen NOTIFY changed)
    Q_PROPERTY(UpdatesModel *updates READ updates CONSTANT)
    Q_PROPERTY(OutputModel *output READ output CONSTANT)

    // list
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString details READ details NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QStringList checkProblems READ checkProblems NOTIFY changed)
    Q_PROPERTY(QString flatpakNote READ flatpakNote NOTIFY changed)
    Q_PROPERTY(bool canUpdate READ canUpdate NOTIFY changed)
    Q_PROPERTY(bool starting READ starting NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

    // running
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(QString progressText READ progressText NOTIFY changed)
    Q_PROPERTY(QString question READ question NOTIFY changed)

    // output view
    Q_PROPERTY(QString commandLine READ commandLine NOTIFY changed)
    Q_PROPERTY(QString processLine READ processLine NOTIFY changed)
    Q_PROPERTY(QString logPath READ logPath NOTIFY changed)

    // finished
    Q_PROPERTY(bool success READ success NOTIFY changed)
    Q_PROPERTY(QString resultTitle READ resultTitle NOTIFY changed)
    Q_PROPERTY(QString resultText READ resultText NOTIFY changed)
    Q_PROPERTY(QString resultDetails READ resultDetails NOTIFY changed)
    Q_PROPERTY(QVariantList changes READ changes NOTIFY changed)

public:
    explicit PanelController(QObject *parent = nullptr);

    // Live sources; all optional so tests can drive the controller directly.
    void setStateWatcher(StateWatcher *watcher);
    void setHelper(HelperClient *helper, bool sessionBus);
    void setCheckScheduler(CheckScheduler *scheduler);

    // Picks up a transaction that is already running (panel opened late or
    // restarted). Returns false if the helper could not be reached.
    bool attach();

    QString screen() const { return m_screen; }
    UpdatesModel *updates() { return &m_updates; }
    OutputModel *output() { return &m_output; }

    QString title() const;
    QString details() const;
    bool checking() const { return m_checking; }
    QStringList checkProblems() const;
    QString flatpakNote() const;
    bool canUpdate() const;
    bool starting() const { return m_starting; }
    QString message() const { return m_message; }

    int percent() const;
    QString progressText() const;
    QString question() const { return m_question; }

    QString commandLine() const { return m_commandLine; }
    QString processLine() const;
    QString logPath() const { return m_logPath; }

    bool success() const { return m_success; }
    QString resultTitle() const;
    QString resultText() const;
    QString resultDetails() const;
    QVariantList changes() const;

public Q_SLOTS:
    void setUpdateState(const Kyu::UpdateState &state, bool checking = false);
    void handleRecord(const QJsonObject &record);
    void handleStartFinished(const QString &transaction, const QString &errorName, const QString &errorMessage);
    void handleHelperLost(const QString &logPath);

    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void checkAgain();
    Q_INVOKABLE void done(); // finished -> list
    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE void copyLog();
    Q_INVOKABLE void followInTerminal();

Q_SIGNALS:
    void changed();
    void activateRequested();

private:
    void setScreen(const QString &screen);
    void reloadFromWatcher();

    StateWatcher *m_watcher = nullptr;
    HelperClient *m_helper = nullptr;
    CheckScheduler *m_scheduler = nullptr;
    bool m_sessionBus = false;
    bool m_helperReachable = true;

    UpdatesModel m_updates;
    OutputModel m_output;
    Kyu::UpdateState m_state;
    bool m_haveState = false;
    bool m_checking = false;
    bool m_listDirty = false; // a new check arrived while a transaction was shown

    QString m_screen = QStringLiteral("list");
    bool m_starting = false;
    QString m_message;

    // transaction
    QString m_tx;
    QString m_commandLine;
    qint64 m_pid = 0;
    qint64 m_uid = -1;
    QString m_logPath;
    QString m_stage;
    QString m_action;
    int m_current = 0;
    int m_total = 0;
    QString m_question;
    QDateTime m_startedAt;
    QDateTime m_finishedAt;
    bool m_success = false;
    QString m_summary;
    int m_exitCode = 0;
    bool m_haveExitCode = false;
};
