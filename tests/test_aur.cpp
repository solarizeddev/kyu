// The AUR check: pacman's version order, the check against a fake pacman and
// a local file standing in for the AUR's web API, and the state.
#include "aur.h"
#include "statustext.h"
#include "traystate.h"
#include "updatestate.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Kyu;

namespace
{
void script(const QString &path, const QByteArray &body)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\n" + body);
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeUser);
}
}

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
        options.pacmanConf = dir.filePath(QStringLiteral("none"));
        options.rpc = QUrl::fromLocalFile(reply).toString();
        const AurResult result = checkAur(options);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.packages.size(), 1);
        QCOMPARE(result.packages.first().name, QStringLiteral("foo-bin"));
        QCOMPARE(result.packages.first().oldVersion, QStringLiteral("1.0-1"));
        QCOMPARE(result.packages.first().newVersion, QStringLiteral("1.1-1"));

        // Held back by pacman (IgnorePkg glob, or an IgnoreGroup member): left out.
        script(dir.filePath(QStringLiteral("pacman-conf")), "case \"$1\" in IgnorePkg) echo 'foo-*';; IgnoreGroup) echo held;; esac\n");
        options.pacmanConf = dir.filePath(QStringLiteral("pacman-conf"));
        QVERIFY(checkAur(options).packages.isEmpty());
        script(dir.filePath(QStringLiteral("pacman-conf")), "case \"$1\" in IgnoreGroup) echo held;; esac\n");
        script(pacman, "case \"$1\" in -Qm) printf 'foo-bin 1.0-1\\nbar-git r10.abc-1\\n';; -Qgq) [ \"$2\" = held ] && echo foo-bin;; *) exit 2;; esac\n");
        QVERIFY(checkAur(options).packages.isEmpty());
        options.pacmanConf = dir.filePath(QStringLiteral("none"));
        QCOMPARE(checkAur(options).packages.size(), 1);

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

    void helperFound()
    {
        // Only Kyu's directory on PATH: no real helper is found.
        QTemporaryDir bin;
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", bin.path().toUtf8());
        const QString conf = bin.filePath(QStringLiteral("pamac.conf"));
        QFile c(conf);
        QVERIFY(c.open(QIODevice::WriteOnly));
        c.write("#EnableAUR\nCheckAURUpdates\n");
        c.close();
        script(bin.filePath(QStringLiteral("pamac")), "exit 0\n");
        QVERIFY(!findAurHelper(conf)); // the AUR not on in pamac
        QVERIFY(c.open(QIODevice::WriteOnly | QIODevice::Truncate));
        c.write("EnableAUR\n#CheckAURUpdates\n");
        c.close();
        QVERIFY(!findAurHelper(conf)); // pamac does not check it: neither does Kyu
        QVERIFY(c.open(QIODevice::WriteOnly | QIODevice::Truncate));
        c.write("EnableAUR\nCheckAURUpdates\n");
        c.close();
        QCOMPARE(findAurHelper(conf)->name, QStringLiteral("pamac"));
        script(bin.filePath(QStringLiteral("yay")), "exit 0\n");
        QCOMPARE(findAurHelper(conf)->name, QStringLiteral("yay"));
        script(bin.filePath(QStringLiteral("paru")), "exit 0\n");
        QCOMPARE(findAurHelper(conf)->name, QStringLiteral("paru"));

        // Kyu's settings: off, or a chosen helper.
        const QString kyuConf = bin.filePath(QStringLiteral("kyu.conf"));
        QCOMPARE(configuredAurHelper(kyuConf, conf)->name, QStringLiteral("paru"));
        QFile k(kyuConf);
        QVERIFY(k.open(QIODevice::WriteOnly));
        k.write("[AUR]\nhelper=yay\n");
        k.close();
        QCOMPARE(configuredAurHelper(kyuConf, conf)->name, QStringLiteral("yay"));
        QVERIFY(k.open(QIODevice::WriteOnly | QIODevice::Truncate));
        k.write("[AUR]\ncheck=false\n");
        k.close();
        QVERIFY(!configuredAurHelper(kyuConf, conf));
        qputenv("PATH", path);
    }

    void failedBuild()
    {
        // The listed version did not build: out of the count until a newer one.
        TrayInput in;
        UpdateState s;
        s.pacman.checked = s.pacman.ok = true;
        s.aurPackages = {{QStringLiteral("foo-bin"), QStringLiteral("aur"), QStringLiteral("1-1"), QStringLiteral("2-1"), -1, false},
                         {QStringLiteral("bar"), QStringLiteral("aur"), QStringLiteral("1"), QStringLiteral("2"), -1, false}};
        in.state = s;
        in.now = QDateTime::currentDateTime();
        in.lastRun = QJsonDocument::fromJson(R"({"pacmanExit":0,"aurExit":1,"aurFailed":[{"name":"foo-bin","version":"2-1"}]})").object();
        leaveOutFailedAur(in);
        QCOMPARE(in.state->aurPackages.size(), 1);
        QCOMPARE(in.aurFailed, QStringList{QStringLiteral("foo-bin")});
        QVERIFY(trayView(in).tooltip.contains(QStringLiteral("AUR: foo-bin did not build")));
        QVERIFY(cardView(in).detail.contains(QStringLiteral("1 AUR did not build")));
        QCOMPARE(trayView(in).mark, TrayMark::Updates);
        // Only that one: up to date, with the line on the card.
        in.state->aurPackages.removeLast();
        QCOMPARE(trayView(in).mark, TrayMark::UpToDate);
        QVERIFY(cardView(in).detail.startsWith(QStringLiteral("AUR: foo-bin did not build")));
        // A newer version: offered again.
        in.state = s;
        in.state->aurPackages.first().newVersion = QStringLiteral("2-2");
        in.aurFailed.clear();
        leaveOutFailedAur(in);
        QCOMPARE(in.state->aurPackages.size(), 2);
    }

    void outage()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        UpdateState s;
        s.checkedAt = now;
        s.aur.checked = true;
        s.aur.ok = false;
        s.aur.error = QStringLiteral("timeout");
        s.aur.lastSuccessAt = now.addSecs(-3600);
        QVERIFY(!statusLines(s, false, {}, now).join(QLatin1Char('\n')).contains(QStringLiteral("AUR"))); // an hour: not said
        s.aur.lastSuccessAt = now.addDays(-2);
        QVERIFY(statusLines(s, false, {}, now).join(QLatin1Char('\n')).contains(QStringLiteral("AUR not checked since")));
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
