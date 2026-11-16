#include "pacmanparse.h"

#include <QTest>

using namespace Kyu;

class TestPacmanParse : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void parsesUpdateLines()
    {
        const QByteArray out =
            "linux618 6.18.20-1 -> 6.18.22-1\n"
            "mesa 1:26.2.1-1 -> 1:26.2.3-1\n"
            "\n";
        const auto packages = PacmanParse::parseCheckupdates(out);
        QCOMPARE(packages.size(), 2);
        QCOMPARE(packages[0], (PackageUpdate{QStringLiteral("linux618"), {}, QStringLiteral("6.18.20-1"), QStringLiteral("6.18.22-1")}));
        QCOMPARE(packages[1].oldVersion, QStringLiteral("1:26.2.1-1"));
        QCOMPARE(packages[1].newVersion, QStringLiteral("1:26.2.3-1"));
    }

    void skipsLinesItDoesNotUnderstand()
    {
        const QByteArray out =
            "kwin 6.7.3-1 -> 6.7.4-1\n"
            "foo 1-1 -> 2-1 [ignored]\n"
            "==> ERROR: something\n"
            "bar 1-1 2-1\n";
        QStringList skipped;
        const auto packages = PacmanParse::parseCheckupdates(out, &skipped);
        QCOMPARE(packages.size(), 1);
        QCOMPARE(packages[0].name, QStringLiteral("kwin"));
        QCOMPARE(skipped.size(), 3);
    }

    void emptyOutput()
    {
        QVERIFY(PacmanParse::parseCheckupdates("").isEmpty());
    }

    void findsRepos()
    {
        const QByteArray syncList =
            "core linux618 6.18.22-1 [installed: 6.18.20-1]\n"
            "core-testing mesa 1:26.3.0-1\n"
            "extra mesa 1:26.2.3-1 [installed: 1:26.2.1-1]\n"
            "extra kwin 6.7.4-1\n"
            "extra-testing kwin 6.7.5-1\n"
            "extra unrelated 1-1\n";
        const QList<PackageUpdate> wanted{
            {QStringLiteral("linux618"), {}, QStringLiteral("6.18.20-1"), QStringLiteral("6.18.22-1")},
            {QStringLiteral("mesa"), {}, QStringLiteral("1:26.2.1-1"), QStringLiteral("1:26.2.3-1")},
            {QStringLiteral("kwin"), {}, QStringLiteral("6.7.3-1"), QStringLiteral("6.7.9-1")},
            {QStringLiteral("missing"), {}, QStringLiteral("1"), QStringLiteral("2")},
        };
        const auto repos = PacmanParse::reposFor(syncList, wanted);
        QCOMPARE(repos.value(QStringLiteral("linux618")), QStringLiteral("core"));
        // Version match beats pacman.conf order.
        QCOMPARE(repos.value(QStringLiteral("mesa")), QStringLiteral("extra"));
        // No version match: first repo in pacman.conf order.
        QCOMPARE(repos.value(QStringLiteral("kwin")), QStringLiteral("extra"));
        QVERIFY(!repos.contains(QStringLiteral("missing")));
        QVERIFY(!repos.contains(QStringLiteral("unrelated")));
    }
};

QTEST_GUILESS_MAIN(TestPacmanParse)
#include "test_pacmanparse.moc"
