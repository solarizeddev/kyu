// The tray's watcher: a check is due once packages stop changing and pacman's
// lock is gone; a Flatpak change counts too.

#include "systemwatcher.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestSystemWatcher : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void packagesSettle();
};

static void touch(const QString &path)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Append));
    f.write("x");
}

void TestSystemWatcher::packagesSettle()
{
    QTemporaryDir dir;
    const QString local = dir.filePath(QStringLiteral("local"));
    QVERIFY(QDir().mkpath(local));
    const QString lock = dir.filePath(QStringLiteral("db.lck"));
    const QString flatpak = dir.filePath(QStringLiteral(".changed"));
    touch(flatpak);

    SystemWatcher::Paths paths;
    paths.pacmanLocal = local;
    paths.pacmanLock = lock;
    paths.flatpakChanged = {flatpak};
    SystemWatcher watcher(paths, 300);
    QSignalSpy spy(&watcher, &SystemWatcher::packagesChanged);

    // A transaction: the lock is held while packages change.
    touch(lock);
    QVERIFY(QDir().mkpath(local + QStringLiteral("/foo-1.0-1")));
    QTest::qWait(800);
    QCOMPARE(spy.count(), 0); // still locked
    QFile::remove(lock);
    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 1); // once, not per package

    // A Flatpak update.
    touch(flatpak);
    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(TestSystemWatcher)
#include "test_systemwatcher.moc"
