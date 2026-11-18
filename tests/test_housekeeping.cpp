// Housekeeping: paccache's list read by its paths only; when the cache is
// worth a line.

#include "housekeeping.h"

#include <QTest>

using namespace Kyu;

class TestHousekeeping : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void paccacheList();
    void cacheThreshold();
    void protectedPackages();
};

void TestHousekeeping::protectedPackages()
{
    const QStringList hold{QStringLiteral("pacman"), QStringLiteral("glibc"), QStringLiteral("manjaro-system")};
    for (const char *name : {"base", "base-devel", "glibc", "linux612", "linux-lts", "linux-firmware", "archlinux-keyring", "manjaro-release", "systemd-sysvcompat", "grub",
                             "mkinitcpio", "sudo"}) {
        QVERIFY2(protectedFromCleanup(QString::fromLatin1(name), hold), name);
    }
    for (const char *name : {"gtk2", "python-pyparsing", "cython", "libutempter", "argon2"}) {
        QVERIFY2(!protectedFromCleanup(QString::fromLatin1(name), hold), name);
    }
    QVERIFY(protectedFromCleanup(QStringLiteral("my-held"), {QStringLiteral("my-held")}));
}

void TestHousekeeping::paccacheList()
{
    // As paccache -d -v -v prints it (C locale), and in German: only the paths count.
    const QByteArray english = "==> Candidate packages:\n/var/cache/pacman/pkg/foo-1.1-1-x86_64.pkg.tar.zst\n"
                               "/var/cache/pacman/pkg/gone-2.0-1-any.pkg.tar.zst.sig\n\n"
                               "==> finished dry run: 2 candidates (disk space saved: 1.95 KiB)\n";
    QCOMPARE(paccacheFiles(english),
             (QStringList{QStringLiteral("/var/cache/pacman/pkg/foo-1.1-1-x86_64.pkg.tar.zst"), QStringLiteral("/var/cache/pacman/pkg/gone-2.0-1-any.pkg.tar.zst.sig")}));
    QVERIFY(paccacheFiles("==> no candidate packages found for pruning\n").isEmpty());
    QVERIFY(paccacheFiles("==> Kandidaten: /nicht/ein/paket\n").isEmpty());
}

void TestHousekeeping::cacheThreshold()
{
    constexpr qint64 MiB = 1024 * 1024;
    Housekeeping h;
    h.cacheBytes = 1024 * MiB;
    QVERIFY(h.cacheWorthCleaning());
    QVERIFY(h.any());
    h.cacheBytes = 600 * MiB; // a usual Manjaro update's old versions, plenty of space
    h.cacheFree = 40 * 1024 * MiB;
    QVERIFY(!h.cacheWorthCleaning());
    QVERIFY(!h.any());
    h.cacheFree = 5 * 1024 * MiB; // a tenth of what is left
    QVERIFY(h.cacheWorthCleaning());
    h.cacheBytes = 90 * MiB; // never for less than 100 MiB
    h.cacheFree = 200 * MiB;
    QVERIFY(!h.cacheWorthCleaning());
    h.cacheFree = -1;
    h.cacheBytes = 600 * MiB;
    QVERIFY(!h.cacheWorthCleaning());
    h.orphans = {QStringLiteral("gtk2")};
    QVERIFY(h.any());
}

QTEST_GUILESS_MAIN(TestHousekeeping)
#include "test_housekeeping.moc"
