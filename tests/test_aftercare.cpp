// Restart detection, pacman's log and failure classification, as kyu-update
// uses them (ported from the old root helper's tests).
#include "aftercare.h"
#include "failure.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Kyu;

namespace
{
void write(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}
}

class TestAftercare : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void kernelReplaced()
    {
        QTemporaryDir dir;
        const QString proc = dir.filePath(QStringLiteral("proc")), modules = dir.filePath(QStringLiteral("modules"));
        write(proc + QStringLiteral("/sys/kernel/osrelease"), "6.18.20-1-MANJARO\n");
        write(proc + QStringLiteral("/cmdline"), "BOOT_IMAGE=/boot/vmlinuz-linux618 root=UUID=x rw\n");
        write(modules + QStringLiteral("/6.18.22-1-MANJARO/vmlinuz"), "");
        write(modules + QStringLiteral("/6.18.22-1-MANJARO/pkgbase"), "linux618\n");
        write(modules + QStringLiteral("/6.12.60-1-MANJARO/vmlinuz"), "");
        write(modules + QStringLiteral("/6.12.60-1-MANJARO/pkgbase"), "linux612\n");
        const KernelState k = kernelState(proc, modules);
        QVERIFY(k.known);
        QVERIFY(k.replaced);
        QCOMPARE(k.running, QStringLiteral("6.18.20-1-MANJARO"));
        QCOMPARE(k.installed, QStringLiteral("6.18.22-1-MANJARO")); // the same package, by pkgbase
    }

    void kernelCurrentOrUnknown()
    {
        QTemporaryDir dir;
        const QString proc = dir.filePath(QStringLiteral("proc")), modules = dir.filePath(QStringLiteral("modules"));
        write(proc + QStringLiteral("/sys/kernel/osrelease"), "6.18.22-1-MANJARO\n");
        QVERIFY(!kernelState(proc, modules).known); // no modules directory: a container
        write(modules + QStringLiteral("/6.18.22-1-MANJARO/vmlinuz"), "");
        const KernelState k = kernelState(proc, modules);
        QVERIFY(k.known);
        QVERIFY(!k.replaced);
    }

    void replacedLibraries()
    {
        QCOMPARE(replacedLibrary(QStringLiteral("7f00-7f01 r--p 00000000 00:1f 9  /usr/lib/libQt6Core.so.6.10.1 (deleted)")),
                 QStringLiteral("/usr/lib/libQt6Core.so.6.10.1"));
        QVERIFY(replacedLibrary(QStringLiteral("7f00-7f01 r--p 00000000 00:1f 9  /usr/lib/libQt6Core.so.6.10.1")).isEmpty());
        QVERIFY(replacedLibrary(QStringLiteral("7f00-7f01 rw-s 00000000 00:01 9  /memfd:wayland-shm (deleted)")).isEmpty());
        QVERIFY(replacedLibrary(QStringLiteral("7f00-7f01 r--p 00000000 00:1f 9  /home/me/.cache/x.so (deleted)")).isEmpty());

        QTemporaryDir dir;
        const QString proc = dir.filePath(QStringLiteral("proc"));
        write(proc + QStringLiteral("/1301/comm"), "plasmashell\n");
        write(proc + QStringLiteral("/1301/maps"), "7f00-7f01 r--p 0 00:1f 9  /usr/lib/libQt6Core.so.6.10.1 (deleted)\n");
        write(proc + QStringLiteral("/1402/comm"), "kate\n");
        write(proc + QStringLiteral("/1402/maps"), "7f00-7f01 r--p 0 00:1f 9  /usr/lib/libQt6Core.so.6.10.2\n");
        const auto found = processesOnReplacedLibraries(::getuid(), proc);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().name, QStringLiteral("plasmashell"));
        QVERIFY(processesOnReplacedLibraries(::getuid() + 1, proc).isEmpty()); // another user's: not listed
    }

    void pacmanLog()
    {
        const PacmanRun run = parsePacmanLog(QStringLiteral(
            "[2026-11-16T14:10:00+0100] [PACMAN] Running 'pacman -Syu'\n"
            "[2026-11-16T14:10:01+0100] [PACMAN] starting full system upgrade\n"
            "[2026-11-16T14:10:20+0100] [ALPM] transaction started\n"
            "[2026-11-16T14:10:21+0100] [ALPM] upgraded linux618 (6.18.20-1 -> 6.18.22-1)\n"
            "[2026-11-16T14:10:22+0100] [ALPM] upgraded systemd (258.1-1 -> 258.2-1)\n"
            "[2026-11-16T14:10:22+0100] [ALPM] installed vulkan-intel (26.2.3-1)\n"
            "[2026-11-16T14:10:22+0100] [ALPM] warning: /etc/sddm.conf installed as /etc/sddm.conf.pacnew\n"
            "[2026-11-16T14:10:22+0100] [ALPM] warning: /etc/foo.conf saved as /etc/foo.conf.pacsave\n"
            "[2026-11-16T14:10:23+0100] [ALPM] removed oldthing (1.0-1)\n"
            "[2026-11-16T14:10:30+0100] [ALPM] transaction completed\n"));
        QVERIFY(run.transactionStarted);
        QVERIFY(run.transactionCompleted);
        QCOMPARE(run.changed, QStringList({QStringLiteral("linux618"), QStringLiteral("systemd"), QStringLiteral("vulkan-intel"), QStringLiteral("oldthing")}));
        QCOMPARE(run.configs, QStringList({QStringLiteral("/etc/sddm.conf.pacnew"), QStringLiteral("/etc/foo.conf.pacsave")}));
        QCOMPARE(restartPackage(run), QStringLiteral("systemd"));

        const PacmanRun nothing = parsePacmanLog(QStringLiteral("[2026-11-16T14:10:01+0100] [PACMAN] starting full system upgrade\n"));
        QVERIFY(!nothing.transactionStarted);
        QVERIFY(nothing.changed.isEmpty());
        QVERIFY(restartPackage(nothing).isEmpty());
    }

    void terminalRecording()
    {
        // Progress bars redraw with \r; colours and the cursor with escapes.
        const QStringList lines = terminalLines(
            "Script started on 2026-11-16\n"
            ":: Retrieving packages...\r\n"
            " mesa  [####----]  40%\r mesa  [########] 100%\r\n"
            "\x1b[1;31merror:\x1b[0m failed retrieving file 'core.db' from mirror.example.org : Could not resolve host: mirror.example.org\r\n");
        QVERIFY(lines.contains(QStringLiteral(" mesa  [########] 100%")));
        QVERIFY(lines.contains(QStringLiteral("error: failed retrieving file 'core.db' from mirror.example.org : Could not resolve host: mirror.example.org")));
    }

    void failureKinds_data()
    {
        QTest::addColumn<QStringList>("lines");
        QTest::addColumn<QString>("kind");
        QTest::newRow("network") << QStringList{QStringLiteral("error: failed retrieving file 'core.db' from mirror.example.org : Could not resolve host: mirror.example.org"),
                                                QStringLiteral("error: failed to synchronize all databases (failed to retrieve some files)")}
                                 << QStringLiteral("network");
        QTest::newRow("mirror") << QStringList{QStringLiteral("error: failed retrieving file 'mesa-1:26.2.3-1-x86_64.pkg.tar.zst' from mirror.example.org : The requested URL returned error: 404")}
                                << QStringLiteral("mirror");
        QTest::newRow("keyring") << QStringList{QStringLiteral("error: mesa: signature from \"Someone <s@example.org>\" is unknown trust"),
                                                QStringLiteral("error: failed to commit transaction (invalid or corrupted package (PGP signature))")}
                                 << QStringLiteral("keyring");
        QTest::newRow("files") << QStringList{QStringLiteral("error: failed to commit transaction (conflicting files)"),
                                              QStringLiteral("foo: /usr/bin/foo exists in filesystem")}
                               << QStringLiteral("files");
        QTest::newRow("disk") << QStringList{QStringLiteral("error: Partition / too full: 61234 blocks needed, 1024 blocks free")} << QStringLiteral("disk");
        QTest::newRow("deps") << QStringList{QStringLiteral("error: failed to prepare transaction (could not satisfy dependencies)"),
                                             QStringLiteral(":: installing foo (2-1) breaks dependency 'foo=1' required by bar")}
                              << QStringLiteral("dependencies");
        QTest::newRow("lock") << QStringList{QStringLiteral("error: failed to init transaction (unable to lock database)")} << QStringLiteral("lock");
        QTest::newRow("unknown") << QStringList{QStringLiteral("error: something new")} << QString();
        QTest::newRow("other language") << QStringList{QStringLiteral("Fehler: Datenbank konnte nicht gesperrt werden")} << QString();
    }
    void failureKinds()
    {
        QFETCH(QStringList, lines);
        QFETCH(QString, kind);
        QCOMPARE(classifyFailure(lines).value(QStringLiteral("kind")).toString(), kind);
    }

    void failureDetails()
    {
        const QJsonObject keyring = classifyFailure({QStringLiteral("error: mesa: signature from \"Someone <s@example.org>\" is unknown trust")});
        QCOMPARE(keyring.value(QStringLiteral("packages")).toArray().first().toString(), QStringLiteral("mesa"));
        const QJsonObject network = classifyFailure(
            {QStringLiteral("error: failed retrieving file 'core.db' from mirror.example.org : Could not resolve host: mirror.example.org")});
        QCOMPARE(network.value(QStringLiteral("hosts")).toArray().first().toString(), QStringLiteral("mirror.example.org"));
    }
};

QTEST_GUILESS_MAIN(TestAftercare)
#include "test_aftercare.moc"
