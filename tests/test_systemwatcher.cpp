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
    void flatpakAppearsLater();
    void networkComesBack();
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

    // A transaction: the lock is held while packages change, one after the
    // other, each within the quiet time.
    touch(lock);
    for (int i = 0; i < 4; ++i) {
        QVERIFY(QDir().mkpath(local + QStringLiteral("/pkg%1-1.0-1").arg(i)));
        QTest::qWait(150);
    }
    QVERIFY(QDir().rmdir(local + QStringLiteral("/pkg0-1.0-1"))); // an upgrade removes the old version
    QTest::qWait(800);
    QCOMPARE(spy.count(), 0); // still locked
    QFile::remove(lock);
    QVERIFY(spy.wait(2000));
    QTest::qWait(500);
    QCOMPARE(spy.count(), 1); // once, not per package

    // A Flatpak update: .changed written in place, then replaced by rename.
    touch(flatpak);
    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 2);
    const QString fresh = dir.filePath(QStringLiteral(".changed.new"));
    touch(fresh);
    QFile::remove(flatpak);
    QVERIFY(QFile::rename(fresh, flatpak));
    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 3);
    // ...and the replaced file is still watched.
    touch(flatpak);
    QVERIFY(spy.wait(2000));
}

void TestSystemWatcher::flatpakAppearsLater()
{
    // No .changed yet (no Flatpak installed): the first install creates it.
    QTemporaryDir dir;
    SystemWatcher::Paths paths;
    paths.pacmanLocal = dir.filePath(QStringLiteral("none"));
    paths.pacmanLock = dir.filePath(QStringLiteral("db.lck"));
    paths.flatpakChanged = {dir.filePath(QStringLiteral(".changed"))};
    SystemWatcher watcher(paths, 300);
    QSignalSpy spy(&watcher, &SystemWatcher::packagesChanged);
    touch(paths.flatpakChanged.constFirst());
    QVERIFY(spy.wait(2000));
}

void TestSystemWatcher::networkComesBack()
{
    SystemWatcher::Paths paths;
    SystemWatcher watcher(paths, 300);
    QSignalSpy spy(&watcher, &SystemWatcher::networkUp);
    // The first report already counts (logged in without a network).
    QVERIFY(QMetaObject::invokeMethod(&watcher, "networkStateChanged", Q_ARG(uint, 70)));
    QCOMPARE(spy.count(), 1);
    QVERIFY(QMetaObject::invokeMethod(&watcher, "networkStateChanged", Q_ARG(uint, 70)));
    QCOMPARE(spy.count(), 1); // no change, no event
    QVERIFY(QMetaObject::invokeMethod(&watcher, "networkStateChanged", Q_ARG(uint, 20)));
    QVERIFY(QMetaObject::invokeMethod(&watcher, "networkStateChanged", Q_ARG(uint, 70)));
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(TestSystemWatcher)
#include "test_systemwatcher.moc"
