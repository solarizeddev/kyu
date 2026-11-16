#include "helperwatcher.h"

#include <QDBusConnection>
#include <QJsonDocument>
#include <QJsonObject>

HelperWatcher::HelperWatcher(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::systemBus().connect(QStringLiteral("io.github.solarizeddev.kyu.Helper"),
                                         QStringLiteral("/io/github/solarizeddev/kyu/Helper"),
                                         QStringLiteral("io.github.solarizeddev.kyu.Helper1"),
                                         QStringLiteral("Event"),
                                         this,
                                         SLOT(onEvent(QString, qulonglong, QString)));
}

void HelperWatcher::onEvent(const QString &, qulonglong, const QString &record)
{
    if (QJsonDocument::fromJson(record.toUtf8()).object().value(QStringLiteral("kind")).toString() == QLatin1String("finished")) {
        Q_EMIT transactionFinished();
    }
}
