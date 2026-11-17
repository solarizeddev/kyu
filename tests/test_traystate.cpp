// What the tray icon shows: one state, most urgent first.

#include "traystate.h"

#include <QTest>

using namespace Kyu;

class TestTrayState : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void states();
    void cards();
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

void TestTrayState::cards()
{
    TrayInput in;
    in.now = QDateTime::fromString(QStringLiteral("2026-11-16T15:00:00Z"), Qt::ISODate);

    UpdateState s = withUpdates(6, true);
    FlatpakUpdate f;
    f.name = QStringLiteral("org.x.App");
    s.flatpaks = {f, f};
    in.state = s;
    CardView c = cardView(in);
    QCOMPARE(c.heading, QStringLiteral("8 updates"));
    QVERIFY(c.detail.startsWith(QStringLiteral("6 packages · 2 Flatpaks")));
    QCOMPARE(c.linkText, QStringLiteral("Read first: Manual intervention for foo"));
    QCOMPARE(c.action, CardView::Action::Update);
    QVERIFY(c.primary);

    // Flatpaks only: no password.
    UpdateState fp = withUpdates(0, false);
    fp.flatpaks = {f, f};
    in.state = fp;
    QCOMPARE(cardView(in).detail, QStringLiteral("2 Flatpaks · no password needed"));

    // Up to date: a quiet button, not a primary one.
    in.state = withUpdates(0, false);
    c = cardView(in);
    QCOMPARE(c.heading, QStringLiteral("Up to date"));
    QCOMPARE(c.action, CardView::Action::Check);
    QVERIFY(!c.primary);

    // Restart: the reason, capitalised.
    in.sameBoot = true;
    in.lastRun = QJsonObject{{QStringLiteral("restartRequired"), true}, {QStringLiteral("restartReason"), QStringLiteral("new kernel 6.18.22-1-MANJARO")},
                             {QStringLiteral("time"), QStringLiteral("2026-11-16T14:12:00Z")}};
    c = cardView(in);
    QCOMPARE(c.heading, QStringLiteral("Restart to finish"));
    QCOMPARE(c.detail, QStringLiteral("New kernel 6.18.22-1-MANJARO"));
    QCOMPARE(c.action, CardView::Action::Restart);

    // Updating: no button at all.
    in.updating = true;
    c = cardView(in);
    QCOMPARE(c.heading, QStringLiteral("Updating…"));
    QCOMPARE(c.action, CardView::Action::None);

    // A failed update: update again, with the kind for the sentence.
    in.updating = false;
    in.sameBoot = false;
    in.state = withUpdates(2, false);
    in.lastRun = QJsonObject{{QStringLiteral("pacmanExit"), 1}, {QStringLiteral("failure"), QStringLiteral("keyring")}, {QStringLiteral("time"), QStringLiteral("2026-11-16T14:30:00Z")}};
    c = cardView(in);
    QCOMPARE(c.heading, QStringLiteral("The update did not finish"));
    QCOMPARE(c.failureKind, QStringLiteral("keyring"));
    QCOMPARE(c.actionText, QStringLiteral("Update again"));

    // Never checked: the check button.
    in = TrayInput{};
    c = cardView(in);
    QCOMPARE(c.heading, QStringLiteral("Not checked yet"));
    QCOMPARE(c.action, CardView::Action::Check);
}

QTEST_GUILESS_MAIN(TestTrayState)
#include "test_traystate.moc"
