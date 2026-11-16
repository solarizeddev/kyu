#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>

// Follows kyu-flatpak's log ($XDG_STATE_HOME/kyu/flatpak/<tx>.jsonl) the way
// HelperClient follows kyu-helper: records in order, from the start of the
// log. kyu-flatpak is a separate process, so closing the panel does not stop
// it, and a panel opened later picks it up with attach().
class FlatpakFollower : public QObject
{
    Q_OBJECT
public:
    explicit FlatpakFollower(QObject *parent = nullptr);

    // Follows the running kyu-flatpak, if there is one.
    bool attach();
    // Starts kyu-flatpak (after pacman transaction `afterTx`, if not empty)
    // and follows it.
    void start(const QString &afterTx, bool sessionBus);

    QString logPath() const
    {
        return m_path;
    }
    // Directory and lock, from $XDG_STATE_HOME.
    static QString logDir();
    static QString lockPath();
    // pid of the running kyu-flatpak, or 0.
    static qint64 runningPid();

Q_SIGNALS:
    void record(const QJsonObject &record);
    void startFailed(const QString &message);
    // kyu-flatpak exited without a "finished" record.
    void runnerLost(const QString &logPath);

private:
    void poll();
    void follow(const QString &path);

    QTimer m_timer;
    QString m_path;
    qint64 m_offset = 0;
    qint64 m_pid = 0; // process to wait for (from start() or the lock)
    bool m_finished = false;
    int m_idlePolls = 0;
};
