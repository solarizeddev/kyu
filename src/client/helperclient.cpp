#include "helperclient.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QJsonDocument>

#include <utility>

namespace
{
// StartUpdate waits for polkit, which waits for the user to type a password.
constexpr int StartTimeoutMs = 10 * 60 * 1000;

QJsonObject parseRecord(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object();
}
}

HelperClient::HelperClient(Bus bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus == Bus::System ? QDBusConnection::systemBus() : QDBusConnection::sessionBus())
{
}

bool HelperClient::subscribe(QString *error)
{
    if (!m_bus.isConnected()) {
        if (error) {
            *error = m_bus.lastError().message();
        }
        return false;
    }
    const bool ok = m_bus.connect(QString::fromLatin1(Service),
                                  QString::fromLatin1(Path),
                                  QString::fromLatin1(Interface),
                                  QStringLiteral("Event"),
                                  this,
                                  SLOT(onEvent(QString, qulonglong, QString)));
    if (!ok && error) {
        *error = m_bus.lastError().message();
    }
    auto *watcher = new QDBusServiceWatcher(QString::fromLatin1(Service), m_bus, QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        // An idle helper exits by design; only an unfinished transaction matters.
        if (m_following && !m_finished) {
            readLog(); // whatever it wrote before going away
            if (!m_finished) {
                Q_EMIT helperLost(m_logPath);
            }
        }
    });
    return ok;
}

std::optional<QJsonObject> HelperClient::state(QString *error)
{
    auto call = QDBusMessage::createMethodCall(QString::fromLatin1(Service),
                                               QString::fromLatin1(Path),
                                               QString::fromLatin1(Interface),
                                               QStringLiteral("GetState"));
    const QDBusMessage reply = m_bus.call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        if (error) {
            *error = reply.errorMessage();
        }
        return std::nullopt;
    }
    return parseRecord(reply.arguments().constFirst().toString().toUtf8());
}

bool HelperClient::follow(const QJsonObject &state)
{
    const QJsonObject tx = state.value(QStringLiteral("transaction")).toObject();
    if (tx.isEmpty()) {
        return false;
    }
    m_tx = tx.value(QStringLiteral("tx")).toString();
    m_logPath = tx.value(QStringLiteral("log")).toString();
    m_following = true;
    readLog();
    const auto early = std::exchange(m_early, {});
    for (const auto &[seq, record] : early) {
        if (record.value(QStringLiteral("tx")).toString() == m_tx) {
            deliver(record, seq);
        }
    }
    return true;
}

void HelperClient::startUpdate()
{
    auto call = QDBusMessage::createMethodCall(QString::fromLatin1(Service),
                                               QString::fromLatin1(Path),
                                               QString::fromLatin1(Interface),
                                               QStringLiteral("StartUpdate"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, StartTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QString> reply = *w;
        if (reply.isError()) {
            Q_EMIT startFinished({}, reply.error().name(), reply.error().message());
            return;
        }
        // The log may not exist yet from our point of view; follow via state.
        QString error;
        if (auto s = state(&error)) {
            follow(*s);
        }
        Q_EMIT startFinished(reply.value(), {}, {});
    });
}

void HelperClient::answer(qulonglong id, const QString &value)
{
    callForReply(QStringLiteral("Answer"), {m_tx, QVariant::fromValue(id), value});
}

void HelperClient::cancel()
{
    callForReply(QStringLiteral("Cancel"), {m_tx});
}

void HelperClient::callForReply(const QString &method, const QVariantList &args)
{
    auto call = QDBusMessage::createMethodCall(QString::fromLatin1(Service),
                                               QString::fromLatin1(Path),
                                               QString::fromLatin1(Interface),
                                               method);
    call.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<> reply = *w;
        Q_EMIT replyFinished(reply.isError() ? reply.error().name() : QString(), reply.isError() ? reply.error().message() : QString());
    });
}

void HelperClient::onEvent(const QString &transaction, qulonglong seq, const QString &recordJson)
{
    const QJsonObject record = parseRecord(recordJson.toUtf8());
    if (!m_following) {
        m_early.append({seq, record});
        return;
    }
    if (transaction != m_tx) {
        return;
    }
    if (seq > m_lastSeq + 1) {
        // Missed signals (or they raced the log read): the log has them.
        readLog();
    }
    deliver(record, seq);
}

void HelperClient::deliver(const QJsonObject &record, qulonglong seq)
{
    if (seq <= m_lastSeq) {
        return; // already delivered from the log
    }
    m_lastSeq = seq;
    if (record.value(QStringLiteral("kind")).toString() == QLatin1String("finished")) {
        m_finished = true;
    }
    Q_EMIT this->record(record);
}

void HelperClient::readLog()
{
    QFile file(m_logPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonObject record = parseRecord(line);
        const auto seq = static_cast<qulonglong>(record.value(QStringLiteral("seq")).toInteger());
        if (seq == m_lastSeq + 1) {
            deliver(record, seq);
        }
    }
}
