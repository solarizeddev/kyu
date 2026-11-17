// The AUR check: pacman's version order, the check against a fake pacman and
// a local file standing in for the AUR's web API, and the state.
#include "aur.h"
#include "updatestate.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Kyu;

class TestAur : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void versions_data()
    {
        QTest::addColumn<QString>("a");
        QTest::addColumn<QString>("b");
        QTest::addColumn<int>("order");
        // From pacman's test/util/vercmptest.sh.
        QTest::newRow("same") << "1.5.0" << "1.5.0" << 0;
        QTest::newRow("minor") << "1.5.1" << "1.5.0" << 1;
        QTest::newRow("release") << "1.5.0-1" << "1.5.0-2" << -1;
        QTest::newRow("release missing") << "1.5.0-1" << "1.5.0" << 0;
        QTest::newRow("alpha") << "1.5.0" << "1.5.0a" << 1;
        QTest::newRow("alpha2") << "1.5a" << "1.5b" << -1;
        QTest::newRow("more parts") << "1.5.1" << "1.5" << 1;
        QTest::newRow("long number") << "1.5.10" << "1.5.9" << 1;
        QTest::newRow("leading zero") << "1.05" << "1.5" << 0;
        QTest::newRow("separators") << "1.0" << "1_0" << 0;
        QTest::newRow("separator length") << "1..0" << "1.0" << 1;
        QTest::newRow("letters vs digits") << "1.0.a" << "1.0.1" << -1;
        QTest::newRow("epoch") << "1:1.0" << "2.0" << 1;
        QTest::newRow("epoch both") << "1:1.0" << "2:1.0" << -1;
        QTest::newRow("epoch zero") << "0:1.0" << "1.0" << 0;
        QTest::newRow("git") << "r123.abc1234-1" << "r124.abc1234-1" << -1;
        QTest::newRow("dash in release") << "1.0-1.1" << "1.0-1" << 1;
    }
    void versions()
    {
        QFETCH(QString, a);
        QFETCH(QString, b);
        QFETCH(int, order);
        const int r = vercmp(a, b);
        QCOMPARE(r < 0 ? -1 : r > 0 ? 1 : 0, order);
        const int back = vercmp(b, a);
        QCOMPARE(back < 0 ? -1 : back > 0 ? 1 : 0, -order);
    }

    void helpers()
    {
        QCOMPARE(aurHelperFor(QStringLiteral("/usr/bin/paru"))->command, (QStringList{QStringLiteral("/usr/bin/paru"), QStringLiteral("-Sua")}));
        QCOMPARE(aurHelperFor(QStringLiteral("yay"))->name, QStringLiteral("yay"));
        QCOMPARE(aurHelperFor(QStringLiteral("pamac"))->command, (QStringList{QStringLiteral("pamac"), QStringLiteral("update"), QStringLiteral("--aur")}));
        QVERIFY(!aurHelperFor(QStringLiteral("trizen")));
    }

    void check()
    {
        QTemporaryDir dir;
        // pacman -Qm: three foreign packages; one the AUR does not know.
        const QString pacman = dir.filePath(QStringLiteral("pacman"));
        QFile p(pacman);
        QVERIFY(p.open(QIODevice::WriteOnly));
        p.write("#!/bin/sh\n[ \"$1\" = -Qm ] || exit 2\nprintf 'foo-bin 1.0-1\\nbar-git r10.abc-1\\nold-dropped 3-1\\n'\n");
        p.close();
        p.setPermissions(p.permissions() | QFileDevice::ExeUser);
        // The AUR's reply: foo-bin newer, bar-git older (its VCS version moved on).
        const QString reply = dir.filePath(QStringLiteral("info.json"));
        QFile r(reply);
        QVERIFY(r.open(QIODevice::WriteOnly));
        r.write(R"({"resultcount":2,"type":"multiinfo","version":5,"results":[
            {"Name":"foo-bin","Version":"1.1-1"},{"Name":"bar-git","Version":"r5.def-1"}]})");
        r.close();
        AurCheckOptions options;
        options.pacman = pacman;
        options.rpc = QUrl::fromLocalFile(reply).toString();
        const AurResult result = checkAur(options);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.packages.size(), 1);
        QCOMPARE(result.packages.first().name, QStringLiteral("foo-bin"));
        QCOMPARE(result.packages.first().oldVersion, QStringLiteral("1.0-1"));
        QCOMPARE(result.packages.first().newVersion, QStringLiteral("1.1-1"));

        // An error reply from the AUR.
        QVERIFY(r.open(QIODevice::WriteOnly | QIODevice::Truncate));
        r.write(R"({"type":"error","error":"Too many package results."})");
        r.close();
        const AurResult failed = checkAur(options);
        QVERIFY(!failed.ok);
        QVERIFY(failed.error.contains(QStringLiteral("Too many")));

        // No foreign packages (pacman -Qm exits 1): fine, nothing asked.
        QVERIFY(p.open(QIODevice::WriteOnly | QIODevice::Truncate));
        p.write("#!/bin/sh\nexit 1\n");
        p.close();
        options.rpc = QStringLiteral("file:///nonexistent");
        const AurResult none = checkAur(options);
        QVERIFY(none.ok);
        QVERIFY(none.packages.isEmpty());
    }

    void state()
    {
        const QDateTime now = QDateTime::fromString(QStringLiteral("2026-11-16T10:00:00Z"), Qt::ISODate);
        AurResult aur;
        aur.ok = true;
        aur.packages = {{QStringLiteral("foo-bin"), QStringLiteral("aur"), QStringLiteral("1-1"), QStringLiteral("2-1"), -1, false}};
        PacmanResult pacman;
        pacman.ok = true;
        pacman.packages = {{QStringLiteral("mesa"), QStringLiteral("extra"), QStringLiteral("1"), QStringLiteral("2"), 10, false}};
        UpdateState s = UpdateState::merge({}, pacman, std::nullopt, now, std::nullopt, aur);
        QCOMPARE(s.total(), 2);
        QCOMPARE(s.summary(), QStringLiteral("2 updates: extra 1 · AUR 1"));
        QCOMPARE(UpdateState::fromJson(QJsonDocument::fromJson(QJsonDocument(s.toJson()).toJson()).object()), s);

        // A failed AUR check keeps the list and is not an error of the update.
        AurResult down;
        down.error = QStringLiteral("aur.archlinux.org: timeout");
        const UpdateState later = UpdateState::merge(s, pacman, std::nullopt, now.addSecs(3600), std::nullopt, down);
        QCOMPARE(later.aurPackages, s.aurPackages);
        QVERIFY(!later.aur.ok);
        QVERIFY(!later.hasError());

        // No helper any more: the AUR is left alone.
        AurResult gone;
        gone.available = false;
        const UpdateState without = UpdateState::merge(s, pacman, std::nullopt, now, std::nullopt, gone);
        QVERIFY(without.aurPackages.isEmpty());
        QVERIFY(!without.aur.checked);
    }
};

QTEST_GUILESS_MAIN(TestAur)
#include "test_aur.moc"
