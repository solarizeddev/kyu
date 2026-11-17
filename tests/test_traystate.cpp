// What the tray icon shows: one state, most urgent first.

#include "traystate.h"

#include <QJsonArray>

#include <QTest>

using namespace Kyu;

class TestTrayState : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void states();
    void cards();
    void details();
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
    QCOMPARE(v.tooltip, QStringLiteral("Restart required: new kernel 6.18.22-1-MANJARO"));
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
    s.news.items.first().link = QStringLiteral("https://forum.manjaro.org/t/1");
    FlatpakUpdate f;
    f.name = QStringLiteral("org.x.App");
    s.flatpaks = {f, f};
    in.state = s;
    CardView c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Updates);
    QCOMPARE(c.heading, QStringLiteral("8 updates available"));
    QCOMPARE(c.icon, QStringLiteral("update-medium")); // news to read
    QVERIFY(c.detail.startsWith(QStringLiteral("6 packages, 2 Flatpaks")));
    QCOMPARE(c.rows.size(), 8);
    QCOMPARE(c.linkText, QStringLiteral("Read first: Manual intervention for foo"));
    QCOMPARE(c.linkUrl, QStringLiteral("https://forum.manjaro.org/t/1"));
    QCOMPARE(c.action, CardView::Action::Update);
    QVERIFY(c.primary);
    // A settings file waiting: not on the updates card (merge after).
    in.configs = {QStringLiteral("/etc/sddm.conf.pacnew")};
    QVERIFY(cardView(in).muted.isEmpty());
    in.configs.clear();
    // While a check runs: the same card.
    in.checking = true;
    QCOMPARE(cardView(in).kind, CardView::Kind::Updates);
    in.checking = false;

    // AUR packages: counted on their own; only AUR still needs a password.
    UpdateState aur = withUpdates(0, false);
    aur.aurPackages = {PackageUpdate{QStringLiteral("foo-bin"), QStringLiteral("aur"), QStringLiteral("1-1"), QStringLiteral("2-1"), -1, false}};
    in.state = aur;
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Updates);
    QCOMPARE(c.heading, QStringLiteral("1 update available"));
    QCOMPARE(c.detail, QStringLiteral("1 AUR"));
    QCOMPARE(c.icon, QStringLiteral("update-low"));
    QCOMPARE(c.rows.value(0).source, QStringLiteral("AUR"));
    QCOMPARE(c.rows.value(0).oldVersion, QStringLiteral("1-1"));
    QCOMPARE(c.rows.value(0).newVersion, QStringLiteral("2-1"));
    QCOMPARE(trayView(in).mark, TrayMark::Updates);

    // A failed check with updates listed still shows the updates.
    UpdateState partly = withUpdates(6, false);
    partly.flatpak.checked = true;
    partly.flatpak.ok = false;
    partly.flatpak.error = QStringLiteral("Unable to connect to dl.flathub.org");
    partly.flatpak.lastSuccessAt = QDateTime::fromString(QStringLiteral("2026-11-15T20:02:00"), Qt::ISODate);
    in.state = partly;
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Updates);
    QCOMPARE(c.muted, QStringLiteral("Could not check for updates · list from yesterday 20:02"));
    QCOMPARE(c.raw, QStringLiteral("Flatpak: Unable to connect to dl.flathub.org"));
    QCOMPARE(trayView(in).mark, TrayMark::Updates);
    // ...with none listed it is "Could not check", with the reason.
    UpdateState none = withUpdates(0, false);
    none.pacman.ok = false;
    none.pacman.error = QStringLiteral("could not resolve host");
    none.pacman.checkedAt = in.now;
    in.state = none;
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::CheckFailed);
    QCOMPARE(c.icon, QStringLiteral("data-warning"));
    // In plain words; the program's own in Details.
    QCOMPARE(c.detail, QStringLiteral("No network connection."));
    QCOMPARE(c.raw, QStringLiteral("pacman: could not resolve host"));
    QCOMPARE(c.action, CardView::Action::Check);
    QCOMPARE(c.actionText, QStringLiteral("Try Again"));
    in.state.reset();
    in.loadError = QStringLiteral("state.json is not JSON");
    c = cardView(in);
    QCOMPARE(c.detail, QStringLiteral("Kyu could not read its list of updates."));
    QCOMPARE(c.raw, QStringLiteral("state.json is not JSON"));
    in.loadError.clear();

    // News already read: no "Read first", no orange tray; the link stays.
    in.state = s;
    in.newsSeen = {QStringLiteral("https://forum.manjaro.org/t/1")};
    QCOMPARE(trayView(in).mark, TrayMark::Updates);
    QCOMPARE(cardView(in).linkText, QStringLiteral("News: Manual intervention for foo"));
    in.newsSeen.clear();
    QCOMPARE(trayView(in).mark, TrayMark::News);
    // Two announcements, the first read: the second is the one to read.
    UpdateState two = s;
    two.news.items.append(NewsItem{QStringLiteral("Second"), QStringLiteral("https://forum.manjaro.org/t/2"), QDateTime()});
    in.state = two;
    in.newsSeen = {QStringLiteral("https://forum.manjaro.org/t/1")};
    c = cardView(in);
    QCOMPARE(c.linkText, QStringLiteral("Read first: Second"));
    QCOMPARE(c.linkKey, QStringLiteral("https://forum.manjaro.org/t/2"));
    QVERIFY(trayView(in).tooltip.startsWith(QStringLiteral("Read first: Second")));
    // The tray's notification: the announcement to read, by its key.
    QCOMPARE(trayView(in).newsTitle, QStringLiteral("Second"));
    QCOMPARE(trayView(in).newsKey, QStringLiteral("https://forum.manjaro.org/t/2"));
    in.newsSeen = {QStringLiteral("https://forum.manjaro.org/t/1"), QStringLiteral("https://forum.manjaro.org/t/2")};
    QVERIFY(trayView(in).newsKey.isEmpty());
    in.newsSeen.clear();

    // Flatpaks only: no password.
    UpdateState fp = withUpdates(0, false);
    fp.flatpaks = {f, f};
    in.state = fp;
    QCOMPARE(cardView(in).detail, QStringLiteral("2 Flatpaks"));

    // Up to date: a quiet button; none while checking.
    in.state = withUpdates(0, false);
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::UpToDate);
    QCOMPARE(c.icon, QStringLiteral("checkmark"));
    QVERIFY(c.detail.isEmpty());
    QVERIFY(c.muted.startsWith(QStringLiteral("Last checked: ")));
    QCOMPARE(c.action, CardView::Action::Check);
    QCOMPARE(c.actionText, QStringLiteral("Check for Updates"));
    QVERIFY(!c.primary);
    // While checking: the button stays, greyed out.
    in.checking = true;
    c = cardView(in);
    QCOMPARE(c.action, CardView::Action::Check);
    QCOMPARE(c.actionText, QStringLiteral("Checking…"));
    QVERIFY(!c.actionEnabled);
    in.checking = false;
    // ...unless a settings file waits: then that, and the tray stays in view.
    in.configs = {QStringLiteral("/etc/sddm.conf.pacnew")};
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Settings);
    QCOMPARE(c.heading, QStringLiteral("1 configuration file to merge"));
    QCOMPARE(c.detail, QStringLiteral("New version: /etc/sddm.conf.pacnew"));
    QVERIFY(c.detailPath);
    QCOMPARE(c.muted, QStringLiteral("Your file stays in use until you merge."));
    QCOMPARE(c.icon, QStringLiteral("merge"));
    QCOMPARE(c.action, CardView::Action::Merge);
    QVERIFY(!trayView(in).passive);
    in.configs.clear();

    // Restart: the recorded reason, capitalised; without a record from this
    // boot, the kernel the system knows.
    in.sameBoot = true;
    in.lastRun = QJsonObject{{QStringLiteral("restartRequired"), true}, {QStringLiteral("restartReason"), QStringLiteral("new kernel 6.18.22-1-MANJARO")},
                             {QStringLiteral("time"), QStringLiteral("2026-11-16T14:12:00Z")}};
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Restart);
    QCOMPARE(c.heading, QStringLiteral("Restart required"));
    QCOMPARE(c.detail, QStringLiteral("Kernel 6.18.22-1-MANJARO is installed."));
    QCOMPARE(c.secondary, CardView::Action::None);
    // A settings file too: noted, and Merge… next to Restart.
    in.configs = {QStringLiteral("/etc/sddm.conf.pacnew")};
    c = cardView(in);
    QCOMPARE(c.muted, QStringLiteral("1 configuration file to merge: /etc/sddm.conf"));
    QCOMPARE(c.secondary, CardView::Action::Merge);
    QCOMPARE(c.action, CardView::Action::Restart);
    in.configs.clear();
    in.lastRun.insert(QStringLiteral("restartReason"), QStringLiteral("systemd was updated"));
    QCOMPARE(cardView(in).detail, QStringLiteral("An update of systemd is installed."));
    in.lastRun.insert(QStringLiteral("restartReason"), QStringLiteral("new kernel 6.18.22-1-MANJARO"));
    in.sameBoot = false;
    in.lastRun = {};
    in.kernelReplaced = true;
    in.kernelInstalled = QStringLiteral("6.18.23-1-MANJARO");
    QCOMPARE(cardView(in).detail, QStringLiteral("Kernel 6.18.23-1-MANJARO is installed."));
    in.kernelReplaced = false;

    // Updating, or merging: busy, and the window can be brought forward.
    in.updating = true;
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Updating);
    QVERIFY(c.busy);
    QCOMPARE(c.action, CardView::Action::ShowWindow);
    QCOMPARE(c.detail, QStringLiteral("Running in the Kyu window."));
    QCOMPARE(c.actionText, QStringLiteral("Show Window"));
    in.activity = QStringLiteral("merge");
    QCOMPARE(cardView(in).kind, CardView::Kind::Merging);
    QCOMPARE(trayView(in).tooltip, QStringLiteral("Merging configuration files"));
    in.updating = false;
    in.activity.clear();

    // A failed update: update again; stamped "tried", with what happened.
    in.state = withUpdates(2, false);
    in.lastRun = QJsonObject{{QStringLiteral("pacmanExit"), 1}, {QStringLiteral("failure"), QStringLiteral("keyring")},
                             {QStringLiteral("transactionStarted"), true}, {QStringLiteral("time"), QStringLiteral("2026-11-16T14:30:00Z")}};
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::Failed);
    QCOMPARE(c.failureKind, QStringLiteral("keyring"));
    QVERIFY(c.transactionStarted);
    QCOMPARE(c.changed, 0);
    QCOMPARE(c.actionText, QStringLiteral("Try Again"));
    QCOMPARE(c.heading, QStringLiteral("Update failed"));
    QCOMPARE(c.icon, QStringLiteral("update-high"));
    QCOMPARE(c.detail, QStringLiteral("The update was interrupted."));
    in.lastRun.insert(QStringLiteral("transactionStarted"), false);
    in.lastRun.insert(QStringLiteral("changed"), 1); // the keyrings went in first
    in.lastRun.insert(QStringLiteral("failureLines"), QJsonArray{QStringLiteral("error: one"), QStringLiteral("error: two")});
    c = cardView(in);
    QCOMPARE(c.changed, 1);
    QCOMPARE(c.detail, QStringLiteral("Only 1 package was upgraded."));
    QCOMPARE(c.raw, QStringLiteral("error: one\nerror: two"));
    in.configs = {QStringLiteral("/etc/a.pacnew"), QStringLiteral("/etc/b.pacnew")};
    QCOMPARE(cardView(in).muted, QStringLiteral("2 configuration files to merge"));
    in.configs.clear();

    // Never checked: the check button.
    in = TrayInput{};
    c = cardView(in);
    QCOMPARE(c.kind, CardView::Kind::NotChecked);
    QCOMPARE(c.action, CardView::Action::Check);
}

void TestTrayState::details()
{
    const QList<DetailRow> rows{{QStringLiteral("linux618"), QStringLiteral("6.18.20-1"), QStringLiteral("6.18.22-1"), QStringLiteral("core")},
                                {QStringLiteral("mesa"), QStringLiteral("1:26.2.1-1"), QStringLiteral("1:26.2.3-1"), QStringLiteral("extra")},
                                {QStringLiteral("visual-studio-code-bin"), QStringLiteral("1.106.0-1"), QStringLiteral("1.106.2-1"), QStringLiteral("AUR")},
                                {QStringLiteral("org.kde.krita"), QString(), QStringLiteral("stable"), QStringLiteral("Flatpak")}};
    const QList<DetailLine> lines = detailLines(rows);
    QStringList text;
    for (const DetailLine &l : lines) {
        text.append(l.text);
    }
    QCOMPARE(text,
             QStringList({QStringLiteral("Package (3)                  Old Version   New Version"),
                          QStringLiteral("core/linux618                6.18.20-1     6.18.22-1"),
                          QStringLiteral("extra/mesa                   1:26.2.1-1    1:26.2.3-1"),
                          QStringLiteral("aur/visual-studio-code-bin   1.106.0-1     1.106.2-1"),
                          QString(),
                          QStringLiteral("Flatpak (1)                  Branch"),
                          QStringLiteral("org.kde.krita                stable")}));
    QVERIFY(lines.at(0).header);
    QVERIFY(!lines.at(1).header);
    QVERIFY(lines.at(5).header);
    // Flatpaks alone: their block only.
    QCOMPARE(detailLines({rows.at(3)}).size(), 2);
    QVERIFY(detailLines({}).isEmpty());
}

QTEST_GUILESS_MAIN(TestTrayState)
#include "test_traystate.moc"
