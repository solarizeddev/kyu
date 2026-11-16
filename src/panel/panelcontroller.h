#pragma once

#include "outputmodel.h"
#include "updatesmodel.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

class FlatpakFollower;
class HelperClient;
class StateWatcher;
class CheckScheduler;

// Everything the panel's QML shows and does. QML only lays it out.
//
// Screens: "list" (pending updates), "running" (a transaction is running,
// started here or elsewhere), "question" (pacman waits for an answer),
// "finished" (result of the transaction this panel followed). The pacman
// output view is a page on top of any screen.
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
    Q_PROPERTY(QString newsTitle READ newsTitle NOTIFY changed)
    Q_PROPERTY(QVariantList newsItems READ newsItems NOTIFY changed)
    Q_PROPERTY(QString newsError READ newsError NOTIFY changed)
    Q_PROPERTY(bool starting READ starting NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

    // running
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(QString progressText READ progressText NOTIFY changed)
    Q_PROPERTY(QString question READ question NOTIFY changed) // one Kyu does not recognise

    // question (pacman waits for an answer)
    Q_PROPERTY(QString questionType READ questionType NOTIFY changed)
    Q_PROPERTY(QString questionTitle READ questionTitle NOTIFY changed)
    Q_PROPERTY(QString questionText READ questionText NOTIFY changed)
    Q_PROPERTY(QString questionPacman READ questionPacman NOTIFY changed)
    Q_PROPERTY(QVariantList questionOptions READ questionOptions NOTIFY changed)
    Q_PROPERTY(QString questionDefault READ questionDefault NOTIFY changed)
    Q_PROPERTY(QString questionNote READ questionNote NOTIFY changed)
    Q_PROPERTY(QString pausedText READ pausedText NOTIFY changed)
    Q_PROPERTY(bool canAnswer READ canAnswer NOTIFY changed)
    Q_PROPERTY(bool replying READ replying NOTIFY changed)

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
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY changed)
    Q_PROPERTY(QString restartReason READ restartReason NOTIFY changed)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY changed)
    Q_PROPERTY(int processCount READ processCount NOTIFY changed)
    Q_PROPERTY(QVariantList configs READ configs NOTIFY changed)

public:
    explicit PanelController(QObject *parent = nullptr);

    // Live sources; all optional so tests can drive the controller directly.
    void setStateWatcher(StateWatcher *watcher);
    void setHelper(HelperClient *helper, bool sessionBus);
    void setCheckScheduler(CheckScheduler *scheduler);
    void setFlatpakFollower(FlatpakFollower *follower);

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
    QString newsTitle() const;
    QVariantList newsItems() const;
    QString newsError() const;
    bool starting() const { return m_starting; }
    QString message() const { return m_message; }

    int percent() const;
    QString progressText() const;
    QString question() const { return m_question; }

    QString questionType() const;
    QString questionTitle() const;
    QString questionText() const;
    QString questionPacman() const;
    QVariantList questionOptions() const;
    QString questionDefault() const;
    QString questionNote() const;
    QString pausedText() const;
    bool canAnswer() const;
    bool replying() const { return m_replying; }

    // The user running the panel; tests set it.
    void setOwnUid(qint64 uid) { m_ownUid = uid; }

    QString commandLine() const { return m_commandLine; }
    QString processLine() const;
    QString logPath() const;

    bool success() const;
    QString resultTitle() const;
    QString resultText() const;
    QString resultDetails() const;
    QVariantList changes() const;
    bool restartRequired() const;
    QString restartReason() const;
    QVariantList processes() const;
    int processCount() const;
    QVariantList configs() const;

    // `diff -u PATH SAVED` as lines {text, kind: hunk/add/remove/context/header},
    // or a single {text, kind: "error"} line.
    Q_INVOKABLE QVariantList diff(const QString &path, const QString &saved) const;

public Q_SLOTS:
    void setUpdateState(const Kyu::UpdateState &state, bool checking = false);
    void handleRecord(const QJsonObject &record);
    void handleStartFinished(const QString &transaction, const QString &errorName, const QString &errorMessage);
    void handleHelperLost(const QString &logPath);
    void handleReplyFinished(const QString &errorName, const QString &errorMessage);
    // kyu-flatpak's records ("source": "flatpak").
    void handleFlatpakRecord(const QJsonObject &record);
    void handleFlatpakLost(const QString &logPath);
    void handleFlatpakStartFailed(const QString &message);

    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void checkAgain();
    Q_INVOKABLE void done(); // finished -> list
    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE void copyLog();
    Q_INVOKABLE void followInTerminal();
    Q_INVOKABLE void answer(const QString &value);
    Q_INVOKABLE void cancelUpdate();
    Q_INVOKABLE void restartNow();
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE void openLink(const QString &url);

Q_SIGNALS:
    void changed();
    void activateRequested();

private:
    void setScreen(const QString &screen);
    void beginRun();
    void finishRun();
    void partFinished();
    void reloadFromWatcher();

    StateWatcher *m_watcher = nullptr;
    HelperClient *m_helper = nullptr;
    CheckScheduler *m_scheduler = nullptr;
    FlatpakFollower *m_flatpak = nullptr;
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

    // the run: pacman part (kyu-helper), then Flatpak part (kyu-flatpak)
    bool m_inRun = false;
    bool m_pacmanRun = false;
    bool m_pacmanDone = false;
    bool m_fpRun = false;
    bool m_fpDone = false;
    bool m_fpSuccess = false;
    bool m_fpSkipped = false;
    QString m_fpSummary;
    QString m_fpStage;
    QString m_fpAction;
    QString m_fpLog;
    qint64 m_fpPid = 0;
    int m_fpCurrent = 0;
    int m_fpTotal = 0;

    // pacman part
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
    QJsonObject m_pending; // the relayed question pacman waits on
    bool m_replying = false;
    QString m_stoppedBy; // "cancelled", "timeout" or "unsupported": pacman stopped at a question
    qint64 m_ownUid = -1;
    QDateTime m_startedAt;
    QDateTime m_finishedAt;
    bool m_success = false;
    QString m_summary;
    QJsonObject m_restart; // from the helper's finished record
    QJsonArray m_configs;
    int m_exitCode = 0;
    bool m_haveExitCode = false;
};
