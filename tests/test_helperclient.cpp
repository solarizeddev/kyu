// HelperClient's log replay across transactions, without a bus.

#include "helperclient.h"

#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestHelperClient : public QObject
{
    Q_OBJECT
private:
    static QString writeLog(const QTemporaryDir &dir, const QString &tx, int records)
    {
        const QString path = dir.filePath(tx + QStringLiteral(".jsonl"));
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            for (int seq = 1; seq <= records; ++seq) {
                const QString kind = seq == records ? QStringLiteral("finished") : QStringLiteral("output");
                f.write(QStringLiteral("{\"v\":1,\"tx\":\"%1\",\"seq\":%2,\"kind\":\"%3\"}\n").arg(tx).arg(seq).arg(kind).toUtf8());
            }
        }
        return path;
    }
    static QJsonObject state(const QString &tx, const QString &log)
    {
        return {{QStringLiteral("phase"), QStringLiteral("finished")},
                {QStringLiteral("transaction"), QJsonObject{{QStringLiteral("tx"), tx}, {QStringLiteral("log"), log}}}};
    }

private Q_SLOTS:
    void secondTransactionIsReplayedFromItsStart()
    {
        QTemporaryDir dir;
        HelperClient client(HelperClient::Bus::Session);
        QSignalSpy records(&client, &HelperClient::record);

        client.follow(state(QStringLiteral("A"), writeLog(dir, QStringLiteral("A"), 5)));
        QCOMPARE(records.size(), 5);

        // A shorter second transaction in the same session (panel kept open).
        client.follow(state(QStringLiteral("B"), writeLog(dir, QStringLiteral("B"), 3)));
        QCOMPARE(records.size(), 8);
        QCOMPARE(records.at(5).at(0).toJsonObject().value(QStringLiteral("tx")).toString(), QStringLiteral("B"));
        QCOMPARE(records.at(5).at(0).toJsonObject().value(QStringLiteral("seq")).toInt(), 1);

        // Following the same transaction again delivers nothing twice.
        client.follow(state(QStringLiteral("B"), dir.filePath(QStringLiteral("B.jsonl"))));
        QCOMPARE(records.size(), 8);
    }
};

QTEST_GUILESS_MAIN(TestHelperClient)
#include "test_helperclient.moc"
