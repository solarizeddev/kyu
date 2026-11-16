// What the tray icon shows: one state, most urgent first.

#include "traystate.h"

#include <QTest>

using namespace Kyu;

class TestTrayState : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void states();
};

static UpdateState withUpdates(int n, bool news)
{
    UpdateState s;
    s.checkedAt = QDateTime::fromString(QStringLiteral("2026-11-16T14:02:00Z"), Qt::ISODate);
    s.pacman.checked = true;
    s.pacman.ok = true;
    for (int i = 0; i < n; ++i) {
        PackageUpdate p;
        p.name = QStringLiteral("p%1").arg(i);
        s.packages.append(p);
    }
    if (news) {
        s.news.items.append(NewsItem{QStringLiteral("Manual intervention for foo"), QString(), QDateTime()});
    }
    return s;
}

void TestTrayState::states()
{
    TrayInput in;
    in.now = QDateTime::fromString(QStringLiteral("2026-11-16T15:00:00Z"), Qt::ISODate);

    in.state = withUpdates(0, false);
    TrayView v = trayView(in);
    QCOMPARE(v.mark, TrayMark::UpToDate);
    QCOMPARE(v.iconName, QStringLiteral("update-none"));
    QVERIFY(v.passive);

    in.state = withUpdates(8, false);
    v = trayView(in);
    QCOMPARE(v.mark, TrayMark::Updates);
    QCOMPARE(v.iconName, QStringLiteral("update-low"));
    QVERIFY(!v.passive);

    in.state = withUpdates(8, true);
    v = trayView(in);
    QCOMPARE(v.mark, TrayMark::News);
    QCOMPARE(v.iconName, QStringLiteral("update-medium"));
    QVERIFY(v.tooltip.startsWith(QStringLiteral("Read first: Manual intervention for foo")));

    // A restart outranks news; only when the record is from this boot.
    in.lastRun = QJsonObject{{QStringLiteral("restartRequired"), true}, {QStringLiteral("restartReason"), QStringLiteral("new kernel 6.18.22-1-MANJARO")}};
    in.sameBoot = false;
    QCOMPARE(trayView(in).mark, TrayMark::News);
    in.sameBoot = true;
    v = trayView(in);
    QCOMPARE(v.mark, TrayMark::Restart);
    QCOMPARE(v.tooltip, QStringLiteral("Restart to finish: new kernel 6.18.22-1-MANJARO"));
    // The kernel alone (an update made without Kyu) also says so.
    in.lastRun = {};
    in.kernelReplaced = true;
    QCOMPARE(trayView(in).mark, TrayMark::Restart);
    in.kernelReplaced = false;

    // A failed check, then a failed update: red.
    UpdateState failed = withUpdates(0, false);
    failed.pacman.ok = false;
    in.state = failed;
    QCOMPARE(trayView(in).iconName, QStringLiteral("update-high"));
    in.state = withUpdates(8, false);
    in.lastRun = QJsonObject{{QStringLiteral("pacmanExit"), 1}, {QStringLiteral("time"), QStringLiteral("2026-11-16T14:30:00Z")}};
    v = trayView(in);
    QCOMPARE(v.mark, TrayMark::Error);
    QCOMPARE(v.tooltip, QStringLiteral("The last update did not finish"));
    // ...until a later check finds nothing left to update.
    UpdateState later = withUpdates(0, false);
    later.checkedAt = QDateTime::fromString(QStringLiteral("2026-11-16T14:45:00Z"), Qt::ISODate);
    in.state = later;
    QCOMPARE(trayView(in).mark, TrayMark::UpToDate);

    // Updating outranks everything; Kyu's own frames, no theme name.
    in.updating = true;
    v = trayView(in);
    QCOMPARE(v.mark, TrayMark::Updating);
    QVERIFY(v.iconName.isEmpty());
}

QTEST_GUILESS_MAIN(TestTrayState)
#include "test_traystate.moc"
