#pragma once

#include <QDBusConnection>
#include <QJsonObject>
#include <QList>
#include <QObject>

#include <optional>

// Talks to kyu-helper over D-Bus and turns its log plus its Event signals
// into one ordered stream of records, without gaps or duplicates.
//
// To follow a transaction that is already running (a panel that opened late,
// or restarted after a crash): attach(). It subscribes to the signals first,
// then reads the log file up to the current sequence number, then delivers
// the signals that arrived meanwhile.
class HelperClient : public QObject
{
    Q_OBJECT
public:
    enum class Bus {
        System,
        Session, // development: kyu-helper --session
    };

    static constexpr const char *Service = "io.github.solarizeddev.kyu.Helper";
    static constexpr const char *Path = "/io/github/solarizeddev/kyu/Helper";
    static constexpr const char *Interface = "io.github.solarizeddev.kyu.Helper1";

    explicit HelperClient(Bus bus, QObject *parent = nullptr);

    // Subscribes to the helper's signals. Call before anything else.
    bool subscribe(QString *error);

    // GetState as JSON (starts the helper through D-Bus activation).
    std::optional<QJsonObject> state(QString *error);

    // Follows the transaction in `state` (if any): replays its log, then live
    // events. Returns false if there is no transaction to follow.
    bool follow(const QJsonObject &state);

    // Asks the helper to run pacman -Syu. polkit may show a password dialog,
    // so this is asynchronous; startFinished() reports the result.
    void startUpdate();

    QString transaction() const
    {
        return m_tx;
    }

Q_SIGNALS:
    void record(const QJsonObject &record);
    // On success `transaction` is set; on failure `errorName` is the D-Bus
    // error (e.g. io.github.solarizeddev.kyu.Helper1.Error.Locked).
    void startFinished(const QString &transaction, const QString &errorName, const QString &errorMessage);
    // The helper left the bus while we followed an unfinished transaction
    // (it crashed or was killed). The log stays where it was.
    void helperLost(const QString &logPath);

private Q_SLOTS:
    void onEvent(const QString &transaction, qulonglong seq, const QString &record);

private:
    void deliver(const QJsonObject &record, qulonglong seq);
    void readLog();

    QDBusConnection m_bus;
    bool m_finished = false;
    QString m_tx;
    QString m_logPath;
    qulonglong m_lastSeq = 0;
    bool m_following = false;
    // Signals that arrived before we knew which transaction to follow.
    QList<std::pair<qulonglong, QJsonObject>> m_early;
};
