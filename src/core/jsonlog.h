#pragma once

#include <QFile>
#include <QJsonObject>
#include <QString>

namespace Kyu
{
// A transaction log in the helper's format (docs/helper-protocol.md): one JSON
// object per line with v, tx, seq, time, kind. Used by kyu-flatpak, which adds
// "source": "flatpak" to every record.
class JsonLog
{
public:
    // Creates <dir>/<tx>.jsonl with a new id (UTC timestamp + counter, sorts
    // by time) and keeps the newest `keep` logs in dir.
    bool create(const QString &dir, const QString &source, QString *error, int keep = 30);

    QString path() const
    {
        return m_file.fileName();
    }
    QString transaction() const
    {
        return m_tx;
    }
    // Appends a record and flushes it; returns the record as written.
    QJsonObject append(const QString &kind, QJsonObject fields = {});

private:
    QFile m_file;
    QString m_tx;
    QString m_source;
    qint64 m_seq = 0;
};
}
