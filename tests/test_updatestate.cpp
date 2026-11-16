#include "statustext.h"
#include "updatestate.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Kyu;

namespace
{
UpdateState sample()
{
    UpdateState s;
    s.checkedAt = QDateTime::fromString(QStringLiteral("2026-11-16T09:00:00Z"), Qt::ISODate);
    s.pacman = {true, true, {}, s.checkedAt, s.checkedAt};
    s.flatpak = {true, true, {}, s.checkedAt, s.checkedAt};
    s.packages = {
        {QStringLiteral("linux618"), QStringLiteral("core"), QStringLiteral("6.18.20-1"), QStringLiteral("6.18.22-1"), 150000000},
        {QStringLiteral("kwin"), QStringLiteral("extra"), QStringLiteral("6.7.3-1"), QStringLiteral("6.7.4-1")},
        {QStringLiteral("mesa"), QStringLiteral("extra"), QStringLiteral("26.2.1-1"), QStringLiteral("26.2.3-1")},
        {QStringLiteral("glibc"), QStringLiteral("core"), QStringLiteral("2.42-1"), QStringLiteral("2.42-2")},
    };
    s.flatpaks = {
        {QStringLiteral("org.mozilla.firefox"), QStringLiteral("app"), QStringLiteral("stable"), QStringLiteral("flathub"), QStringLiteral("system")},
    };
    return s;
}
}

class TestUpdateState : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void groupsInFirstSeenOrder()
    {
        const auto groups = sample().groups();
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups[0], std::make_pair(QStringLiteral("core"), 2));
        QCOMPARE(groups[1], std::make_pair(QStringLiteral("extra"), 2));
        QCOMPARE(groups[2], std::make_pair(QStringLiteral("flathub"), 1));
    }

    void summary()
    {
        QCOMPARE(sample().summary(), QStringLiteral("5 updates: core 2 · extra 2 · flathub 1"));
        QCOMPARE(UpdateState{}.summary(), QStringLiteral("No updates"));
        UpdateState one;
        one.packages = {{QStringLiteral("a"), {}, QStringLiteral("1"), QStringLiteral("2")}};
        QCOMPARE(one.summary(), QStringLiteral("1 update: unknown 1"));
    }

    void jsonRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/updates.json"));
        const auto original = sample();
        QString error;
        QVERIFY2(original.save(path, &error), qPrintable(error));
        const auto loaded = UpdateState::load(path, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(*loaded, original);
    }

    void rejectsOtherFormatVersions()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("updates.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"formatVersion": 99})");
        file.close();
        QString error;
        QVERIFY(!UpdateState::load(path, &error));
        QVERIFY(error.contains(QStringLiteral("99")));
    }

    void failedSourceKeepsPreviousList()
    {
        const auto previous = sample();
        const QDateTime later = previous.checkedAt.addSecs(3600);
        PacmanResult failed;
        failed.error = QStringLiteral("==> ERROR: Cannot fetch updates");
        FlatpakResult fresh;
        fresh.ok = true;

        const auto merged = UpdateState::merge(previous, failed, fresh, later);
        QCOMPARE(merged.packages, previous.packages);
        QVERIFY(!merged.pacman.ok);
        QCOMPARE(merged.pacman.error, failed.error);
        QCOMPARE(merged.pacman.checkedAt, later);
        QCOMPARE(merged.pacman.lastSuccessAt, previous.checkedAt);
        QVERIFY(merged.flatpaks.isEmpty());
        QCOMPARE(merged.flatpak.lastSuccessAt, later);
        QVERIFY(merged.hasError());
    }

    void skippedSourceIsUntouched()
    {
        const auto previous = sample();
        const auto merged = UpdateState::merge(previous, std::nullopt, std::nullopt, previous.checkedAt.addSecs(60));
        QCOMPARE(merged.packages, previous.packages);
        QCOMPARE(merged.pacman, previous.pacman);
    }

    void unavailableFlatpakIsCleared()
    {
        FlatpakResult unavailable;
        unavailable.available = false;
        const auto merged = UpdateState::merge(sample(), std::nullopt, unavailable, QDateTime::currentDateTimeUtc());
        QVERIFY(merged.flatpaks.isEmpty());
        QVERIFY(!merged.flatpak.checked);
        QVERIFY(!merged.hasError());
    }

    void statusLines()
    {
        const QDateTime now = QDateTime::currentDateTime();
        QCOMPARE(Kyu::statusLines(std::nullopt, false, {}, now), QStringList{QStringLiteral("Not checked yet")});
        QCOMPARE(Kyu::statusLines(std::nullopt, true, {}, now), QStringList{QStringLiteral("Checking for updates")});

        auto state = sample();
        state.checkedAt = now;
        state.pacman.ok = false;
        state.pacman.checkedAt = now;
        state.pacman.error = QStringLiteral("==> ERROR: Cannot fetch updates\nsecond line");
        const auto lines = Kyu::statusLines(state, false, {}, now);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0], state.summary());
        QVERIFY(lines[1].startsWith(QStringLiteral("Checked ")));
        QVERIFY(lines[2].startsWith(QStringLiteral("pacman check failed at ")));
        QVERIFY(lines[2].contains(QStringLiteral("Cannot fetch updates; list from ")));
        QVERIFY(!lines[2].contains(QStringLiteral("second line")));
    }
};

QTEST_GUILESS_MAIN(TestUpdateState)
#include "test_updatestate.moc"
