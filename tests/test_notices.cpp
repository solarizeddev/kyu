// The tray's "Updates" notification: none → some, at most once in 20 hours.

#include "notices.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Kyu;

class TestNotices : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noneToSome();
    void growingIsQuiet();
    void onceInTwentyHours();
    void failedCheckChangesNothing();
    void updatingCloses();
    void checkingWaits();
    void clock();
    void unreadNewsFirst();
    void text();
    void memoryFile();
};

static QDateTime at(const char *iso)
{
    return QDateTime::fromString(QString::fromLatin1(iso), Qt::ISODate);
}

static UpdateState updates(const QStringList &names, int flatpaks = 0, int aur = 0)
{
    UpdateState s;
    s.pacman.checked = true;
    s.pacman.ok = true;
    for (const QString &name : names) {
        PackageUpdate p;
        p.name = name;
        s.packages.append(p);
    }
    for (int i = 0; i < flatpaks; ++i) {
        s.flatpaks.append(FlatpakUpdate{QStringLiteral("org.x.App%1").arg(i), QStringLiteral("app"), QStringLiteral("stable"), QStringLiteral("flathub"), QStringLiteral("system")});
    }
    for (int i = 0; i < aur; ++i) {
        PackageUpdate p;
        p.name = QStringLiteral("aur%1").arg(i);
        p.repo = QStringLiteral("aur");
        s.aurPackages.append(p);
    }
    return s;
}

static TrayInput input(const UpdateState &s, const char *now)
{
    TrayInput in;
    in.state = s;
    in.now = at(now);
    return in;
}

void TestNotices::noneToSome()
{
    UpdatesMemory m;
    QCOMPARE(updatesNotice(input(updates({}), "2026-11-16T08:00:00Z"), m).action, UpdatesNotice::Do::Close);
    QVERIFY(!m.told);
    const UpdatesNotice n = updatesNotice(input(updates({QStringLiteral("a"), QStringLiteral("b")}), "2026-11-16T09:00:00Z"), m);
    QCOMPARE(n.action, UpdatesNotice::Do::Send);
    QCOMPARE(n.title, QStringLiteral("2 updates available"));
    QCOMPARE(n.text, QStringLiteral("2 packages"));
    QVERIFY(m.told);
    QCOMPARE(m.sentAt, at("2026-11-16T09:00:00Z"));
}

void TestNotices::growingIsQuiet()
{
    UpdatesMemory m;
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("a")}), "2026-11-16T09:00:00Z"), m).action, UpdatesNotice::Do::Send);
    // Grows, shrinks, a day later: nothing.
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("a"), QStringLiteral("b")}), "2026-11-17T12:00:00Z"), m).action, UpdatesNotice::Do::Nothing);
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("b")}), "2026-11-18T12:00:00Z"), m).action, UpdatesNotice::Do::Nothing);
}

void TestNotices::onceInTwentyHours()
{
    UpdatesMemory m;
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("a")}), "2026-11-16T09:00:00Z"), m).action, UpdatesNotice::Do::Send);
    // Updated (none), new ones at noon: too soon, held back, not dropped.
    QCOMPARE(updatesNotice(input(updates({}), "2026-11-16T10:00:00Z"), m).action, UpdatesNotice::Do::Close);
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("b")}), "2026-11-16T12:00:00Z"), m).action, UpdatesNotice::Do::Nothing);
    QVERIFY(!m.told);
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("b")}), "2026-11-17T04:59:59Z"), m).action, UpdatesNotice::Do::Nothing);
    // 20 hours after the last one: told, once.
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("b")}), "2026-11-17T05:00:00Z"), m).action, UpdatesNotice::Do::Send);
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("b"), QStringLiteral("c")}), "2026-11-18T12:00:00Z"), m).action, UpdatesNotice::Do::Nothing);
    // Emptied within the window, filled again after it: told.
    updatesNotice(input(updates({}), "2026-11-17T06:00:00Z"), m);
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("d")}), "2026-11-17T07:00:00Z"), m).action, UpdatesNotice::Do::Nothing);
    updatesNotice(input(updates({}), "2026-11-17T08:00:00Z"), m);
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("e")}), "2026-11-18T07:00:00Z"), m).action, UpdatesNotice::Do::Send);
}

void TestNotices::failedCheckChangesNothing()
{
    UpdatesMemory m;
    TrayInput in = input(updates({}), "2026-11-16T09:00:00Z");
    in.state->pacman.ok = false; // none listed, but the check failed
    m.told = true;
    QCOMPARE(updatesNotice(in, m).action, UpdatesNotice::Do::Close);
    QVERIFY(m.told); // not known to be none
    in = input(updates({QStringLiteral("a")}), "2026-11-16T09:00:00Z");
    in.loadError = QStringLiteral("unreadable");
    UpdatesMemory fresh;
    QCOMPARE(updatesNotice(in, fresh).action, UpdatesNotice::Do::Nothing);
    QCOMPARE(fresh, UpdatesMemory());
    in.loadError.clear();
    in.state.reset();
    QCOMPARE(updatesNotice(in, fresh).action, UpdatesNotice::Do::Nothing);
    QCOMPARE(fresh, UpdatesMemory());
}

void TestNotices::updatingCloses()
{
    UpdatesMemory m{true, at("2026-11-16T08:00:00Z")};
    const UpdatesMemory before = m;
    TrayInput in = input(updates({QStringLiteral("a")}), "2026-11-16T09:00:00Z");
    in.updating = true;
    QCOMPARE(updatesNotice(in, m).action, UpdatesNotice::Do::Close);
    QCOMPARE(m, before);
    // Only held-back AUR packages (left out of the state): nothing listed.
    UpdatesMemory fresh;
    QCOMPARE(updatesNotice(input(updates({}), "2026-11-16T09:00:00Z"), fresh).action, UpdatesNotice::Do::Close);
}

void TestNotices::checkingWaits()
{
    // kyu-check saves pacman's list, then again with the AUR's: told once it
    // is done, with the whole count.
    UpdatesMemory m;
    TrayInput in = input(updates({QStringLiteral("a")}), "2026-11-16T09:00:00Z");
    in.checking = true;
    QCOMPARE(updatesNotice(in, m).action, UpdatesNotice::Do::Nothing);
    QCOMPARE(m, UpdatesMemory());
    in = input(updates({QStringLiteral("a")}, 0, 1), "2026-11-16T09:00:05Z");
    const UpdatesNotice n = updatesNotice(in, m);
    QCOMPARE(n.action, UpdatesNotice::Do::Send);
    QCOMPARE(n.title, QStringLiteral("2 updates available"));
}

void TestNotices::clock()
{
    // Sent "tomorrow" by a clock that was wrong: not counted.
    UpdatesMemory m{false, at("2026-11-20T09:00:00Z")};
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("a")}), "2026-11-16T09:00:00Z"), m).action, UpdatesNotice::Do::Send);
    // Times are kept in UTC: a local time is compared by its instant.
    UpdatesMemory local{false, at("2026-11-16T09:00:00+01:00")};
    QCOMPARE(updatesNotice(input(updates({QStringLiteral("a")}), "2026-11-17T04:00:00Z"), local).action, UpdatesNotice::Do::Send);
    QCOMPARE(local.sentAt.timeSpec(), Qt::UTC);
}

void TestNotices::unreadNewsFirst()
{
    UpdatesMemory m;
    UpdateState s = updates({QStringLiteral("a")});
    s.news.items.append(NewsItem{QStringLiteral("Manual intervention"), QString(), QDateTime()});
    TrayInput in = input(s, "2026-11-16T09:00:00Z");
    QCOMPARE(updatesNotice(in, m).action, UpdatesNotice::Do::Nothing);
    QCOMPARE(m, UpdatesMemory()); // held back, not dropped
    // Read: the updates are told now.
    in.newsSeen = {newsKey(s.news.items.constFirst())};
    in.now = at("2026-11-17T09:00:00Z");
    QCOMPARE(updatesNotice(in, m).action, UpdatesNotice::Do::Send);
}

void TestNotices::text()
{
    UpdatesMemory m;
    UpdateState s = updates({QStringLiteral("linux612"), QStringLiteral("linux612-headers"), QStringLiteral("linux612-nvidia"), QStringLiteral("mesa"),
                             QStringLiteral("plasma-workspace"), QStringLiteral("zlib")},
                            2, 1);
    UpdatesNotice n = updatesNotice(input(s, "2026-11-16T09:00:00Z"), m);
    QCOMPARE(n.title, QStringLiteral("9 updates available"));
    QCOMPARE(n.text, QStringLiteral("6 packages, 1 AUR, 2 Flatpaks · including the kernel, Plasma and Mesa"));

    QCOMPARE(notableUpdates(updates({QStringLiteral("linux-lts"), QStringLiteral("systemd")})), (QStringList{QStringLiteral("the kernel"), QStringLiteral("systemd")}));
    QCOMPARE(notableUpdates(updates({QStringLiteral("linux-firmware"), QStringLiteral("linux-api-headers"), QStringLiteral("nvidia-utils")})),
             QStringList{QStringLiteral("the NVIDIA driver")});
    QCOMPARE(notableUpdates(updates({QStringLiteral("nvidia-470xx-utils")})), QStringList{QStringLiteral("the NVIDIA driver")});
    QVERIFY(notableUpdates(updates({QStringLiteral("linux612-nvidia"), QStringLiteral("lib32-nvidia-utils")})).isEmpty());
    UpdateState one = updates({}, 1);
    UpdatesMemory m2;
    n = updatesNotice(input(one, "2026-11-16T09:00:00Z"), m2);
    QCOMPARE(n.title, QStringLiteral("1 update available"));
    QCOMPARE(n.text, QStringLiteral("1 Flatpak"));
}

void TestNotices::memoryFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("state/updates-notified.json"));
    QCOMPARE(readUpdatesMemory(path), UpdatesMemory());
    // Written in UTC whatever the zone it came in.
    UpdatesMemory m{true, at("2026-11-16T10:00:00+01:00")};
    QVERIFY(writeUpdatesMemory(m, path));
    const UpdatesMemory back = readUpdatesMemory(path);
    QVERIFY(back.told);
    QCOMPARE(back.sentAt, at("2026-11-16T09:00:00Z"));
    QCOMPARE(back.sentAt.timeSpec(), Qt::UTC);
    // Corrupt: as none.
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not json");
    file.close();
    QCOMPARE(readUpdatesMemory(path), UpdatesMemory());
}

QTEST_GUILESS_MAIN(TestNotices)
#include "test_notices.moc"
