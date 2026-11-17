#include "failuretext.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

class TestFailureText : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // A damaged download: the fix removes the cached files, as root, so
    // only plain cache paths, each quoted (the names come from pacman's
    // output and the mirror's package list).
    void corruptFixOnlyForPlainCachePaths()
    {
        const auto failure = [](const QStringList &files) {
            return QJsonObject{{QStringLiteral("kind"), QStringLiteral("corrupt")},
                               {QStringLiteral("packages"), QJsonArray{QStringLiteral("zlib")}},
                               {QStringLiteral("files"), QJsonArray::fromStringList(files)}};
        };
        FailureText t = describeFailure(failure({QStringLiteral("/var/cache/pacman/pkg/zlib-1:1.4-1-x86_64.pkg.tar.zst")}));
        QCOMPARE(t.title, QStringLiteral("A downloaded package is damaged"));
        QCOMPARE(t.fix, QStringLiteral("sudo rm -f '/var/cache/pacman/pkg/zlib-1:1.4-1-x86_64.pkg.tar.zst'"));
        t = describeFailure(failure({QStringLiteral("/var/cache/pacman/pkg/x$(touch /tmp/owned).pkg.tar.zst")}));
        QVERIFY(t.fix.isEmpty());
        t = describeFailure(failure({QStringLiteral("/etc/passwd")}));
        QVERIFY(t.fix.isEmpty());
        t = describeFailure(failure({}));
        QVERIFY(t.fix.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFailureText)
#include "test_failuretext.moc"
