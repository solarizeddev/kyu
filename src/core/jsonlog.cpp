#include "jsonlog.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>

namespace Kyu
{
bool JsonLog::create(const QString &dir, const QString &source, QString *error, int keep)
{
    QDir d;
    if (!d.mkpath(dir)) {
        if (error) {
            *error = QStringLiteral("cannot create %1").arg(dir);
        }
        return false;
    }
    QStringList old = QDir(dir).entryList({QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
    while (old.size() >= keep) {
        QFile::remove(dir + QLatin1Char('/') + old.takeFirst());
    }
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    for (int attempt = 0; attempt < 100; ++attempt) {
        const QString tx = QStringLiteral("%1-%2").arg(stamp).arg(attempt, 2, 10, QLatin1Char('0'));
        m_file.setFileName(dir + QLatin1Char('/') + tx + QStringLiteral(".jsonl"));
        if (m_file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            m_tx = tx;
            m_source = source;
            m_seq = 0;
            return true;
        }
        if (m_file.error() != QFileDevice::OpenError || !m_file.exists()) {
            break;
        }
    }
    if (error) {
        *error = m_file.errorString();
    }
    return false;
}

QJsonObject JsonLog::append(const QString &kind, QJsonObject fields)
{
    fields.insert(QStringLiteral("v"), 1);
    fields.insert(QStringLiteral("tx"), m_tx);
    fields.insert(QStringLiteral("seq"), ++m_seq);
    fields.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    fields.insert(QStringLiteral("kind"), kind);
    if (!m_source.isEmpty()) {
        fields.insert(QStringLiteral("source"), m_source);
    }
    if (m_file.isOpen()) {
        m_file.write(QJsonDocument(fields).toJson(QJsonDocument::Compact));
        m_file.write("\n");
        m_file.flush();
    }
    return fields;
}
}
