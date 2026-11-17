// Runs the kyu-check binary against fake checkupdates/pacman scripts.

#include "updatestate.h"

#include <QDir>
#include <QFile>
#include <QUrl>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace Kyu;

class TestKyuCheck : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_dir;

    QString writeScript(const QString &name, const QByteArray &body)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return {};
        }
        file.write("#!/bin/sh\n" + body);
        file.close();
        file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
        return path;
    }

    QString statePath() const
    {
        return m_dir.filePath(QStringLiteral("state/updates.json"));
    }

    int run(const QString &checkupdates, QByteArray *stdErr = nullptr)
    {
        QProcess p;
        p.start(QStringLiteral(KYU_CHECK_BIN),
                {QStringLiteral("--no-flatpak"), QStringLiteral("--no-aur"),
                 QStringLiteral("--no-news"),
                 QStringLiteral("--state-file"), statePath(),
                 QStringLiteral("--db-path"), m_dir.filePath(QStringLiteral("db")),
                 QStringLiteral("--checkupdates"), checkupdates,
                 QStringLiteral("--pacman"), m_dir.filePath(QStringLiteral("pacman"))});
        if (!p.waitForFinished(30000)) {
            return -1;
        }
        if (stdErr) {
            *stdErr = p.readAllStandardError();
        }
        return p.exitCode();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // Fake pacman -Sl. Fails unless called with the db path kyu-check gave checkupdates.
        QVERIFY(!writeScript(QStringLiteral("pacman"),
                             "[ \"$2\" = --dbpath ] && [ -d \"$3\" ] || exit 1\n"
                             "if [ \"$1\" = -Sl ]; then\n"
                             "  echo 'core linux618 6.18.22-1 [installed: 6.18.20-1]'\n"
                             "  echo 'extra kwin 6.7.4-1'\n"
                             "elif [ \"$1\" = -Sup ] && [ \"$4 $5 $6\" = '--print-format %n %s --noconfirm' ]; then\n"
                             "  echo 'linux618 150000000'\n"
                             "  echo 'kwin 9000000'\n"
                             "  echo 'new-dependency 1000'\n"
                             "else exit 1; fi\n")
                     .isEmpty());
    }

    void listsUpdatesWithRepos()
    {
        // Checks the environment kyu-check promises checkupdates.
        const QString script = writeScript(QStringLiteral("checkupdates-ok"),
                                           "[ \"$LC_ALL\" = C ] && [ -n \"$CHECKUPDATES_DB\" ] || { echo bad env >&2; exit 1; }\n"
                                           "echo 'kwin 6.7.3-1 -> 6.7.4-1'\n"
                                           "echo 'linux618 6.18.20-1 -> 6.18.22-1'\n"
                                           "echo 'orphan 1-1 -> 2-1'\n");
        QByteArray err;
        QCOMPARE(run(script, &err), 0);
        const auto state = UpdateState::load(statePath());
        QVERIFY(state);
        QCOMPARE(state->packages.size(), 3);
        // Sorted into pacman.conf repo order (core before extra), unknown last.
        QCOMPARE(state->packages[0].name, QStringLiteral("linux618"));
        QCOMPARE(state->packages[0].repo, QStringLiteral("core"));
        QCOMPARE(state->packages[1].repo, QStringLiteral("extra"));
        QCOMPARE(state->packages[2].repo, QString());
        QCOMPARE(state->packages[0].size, 150000000);
        QCOMPARE(state->packages[2].size, -1);
        QCOMPARE(state->totalSize(), 159000000);
        QVERIFY(state->pacman.ok);
        QVERIFY(!state->flatpak.checked);
    }

    void failureKeepsListAndQuotesError()
    {
        const QString script = writeScript(QStringLiteral("checkupdates-fail"),
                                           "echo '==> ERROR: Cannot fetch updates' >&2\nexit 1\n");
        QByteArray err;
        QCOMPARE(run(script, &err), 2);
        QVERIFY(err.contains("Cannot fetch updates"));
        const auto state = UpdateState::load(statePath());
        QVERIFY(state);
        QCOMPARE(state->packages.size(), 3);
        QVERIFY(!state->pacman.ok);
        QCOMPARE(state->pacman.error, QStringLiteral("==> ERROR: Cannot fetch updates"));
        QVERIFY(state->pacman.lastSuccessAt.isValid());
    }

    void missingBinaryIsAnError()
    {
        QCOMPARE(run(m_dir.filePath(QStringLiteral("does-not-exist"))), 2);
        const auto state = UpdateState::load(statePath());
        QVERIFY(state);
        QVERIFY(state->pacman.error.contains(QStringLiteral("does-not-exist")));
    }

    // A lock left in Kyu's private database copy (a check killed) is
    // removed first; it failed every later check (seen in a real run).
    void staleLockInPrivateCopy()
    {
        QDir().mkpath(m_dir.filePath(QStringLiteral("db")));
        QFile lock(m_dir.filePath(QStringLiteral("db/db.lck")));
        QVERIFY(lock.open(QIODevice::WriteOnly));
        lock.close();
        // Like checkupdates: fails while the lock is there.
        const QString script = writeScript(QStringLiteral("checkupdates-lock"), "[ -e \"$CHECKUPDATES_DB/db.lck\" ] && exit 1\nexit 2\n");
        QCOMPARE(run(script), 0);
        const auto state = UpdateState::load(statePath());
        QVERIFY(state);
        QVERIFY(state->pacman.ok);
    }

    void noUpdates()
    {
        QCOMPARE(run(writeScript(QStringLiteral("checkupdates-none"), "exit 2\n")), 0);
        const auto state = UpdateState::load(statePath());
        QVERIFY(state);
        QVERIFY(state->packages.isEmpty());
        QVERIFY(state->pacman.ok);
        QVERIFY(state->pacman.error.isEmpty());
    }

    void restartFlagAndNews()
    {
        // The running kernel's modules directory names its package.
        QDir().mkpath(m_dir.filePath(QStringLiteral("modules/6.18.20-1-MANJARO")));
        QFile pkgbase(m_dir.filePath(QStringLiteral("modules/6.18.20-1-MANJARO/pkgbase")));
        QVERIFY(pkgbase.open(QIODevice::WriteOnly));
        pkgbase.write("linux618\n");
        pkgbase.close();
        const QString osRelease = writeScript(QStringLiteral("os-release"), "");
        QFile os(osRelease);
        QVERIFY(os.open(QIODevice::WriteOnly | QIODevice::Truncate));
        os.write("ID=arch\n");
        os.close();
        QFile log(m_dir.filePath(QStringLiteral("pacman.log")));
        QVERIFY(log.open(QIODevice::WriteOnly));
        log.write("[2026-11-10T20:30:00+0000] [PACMAN] starting full system upgrade\n"
                  "[2026-11-10T20:31:00+0000] [ALPM] transaction completed\n");
        log.close();
        QFile feed(m_dir.filePath(QStringLiteral("feed.xml")));
        QVERIFY(feed.open(QIODevice::WriteOnly));
        feed.write("<rss><channel><item><title>New</title><link>n</link><pubDate>Sat, 14 Nov 2026 10:00:00 +0000</pubDate></item>"
                   "<item><title>Old</title><link>o</link><pubDate>Sat, 07 Nov 2026 10:00:00 +0000</pubDate></item></channel></rss>");
        feed.close();

        const QString checkupdates = writeScript(QStringLiteral("checkupdates-kernel"),
                                                 "echo 'linux618 6.18.20-1 -> 6.18.22-1'\n"
                                                 "echo 'linux618-headers 6.18.20-1 -> 6.18.22-1'\n");
        QProcess p;
        p.start(QStringLiteral(KYU_CHECK_BIN),
                {QStringLiteral("--no-flatpak"), QStringLiteral("--no-aur"), QStringLiteral("--state-file"), statePath(), QStringLiteral("--db-path"),
                 m_dir.filePath(QStringLiteral("db")), QStringLiteral("--checkupdates"), checkupdates, QStringLiteral("--pacman"),
                 m_dir.filePath(QStringLiteral("pacman")), QStringLiteral("--modules-dir"), m_dir.filePath(QStringLiteral("modules")),
                 QStringLiteral("--kernel-release"), QStringLiteral("6.18.20-1-MANJARO"), QStringLiteral("--os-release"), osRelease,
                 QStringLiteral("--pacman-log"), m_dir.filePath(QStringLiteral("pacman.log")), QStringLiteral("--news-feed"),
                 QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("feed.xml"))).toString()});
        QVERIFY(p.waitForFinished(30000));
        QCOMPARE(p.exitCode(), 0);
        const auto state = UpdateState::load(statePath());
        QVERIFY(state);
        QVERIFY(state->restartNeeded());
        QVERIFY(state->packages[0].restart); // linux618
        QVERIFY(!state->packages[1].restart); // headers: not the running kernel
        QVERIFY(state->news.ok);
        QCOMPARE(state->news.source, QStringLiteral("Arch Linux news"));
        QCOMPARE(state->news.items.size(), 1);
        QCOMPARE(state->news.items[0].title, QStringLiteral("New"));
    }

    void aurAskedEverySixHours()
    {
        // A helper named paru, a pacman with one AUR package, an AUR reply.
        const QString helper = writeScript(QStringLiteral("paru"), "exit 0\n");
        const QString pacman = writeScript(QStringLiteral("aur-pacman"), "[ \"$1\" = -Qm ] && cat \"$(dirname \"$0\")/aur-local\"\n");
        const auto write = [this](const QString &name, const QByteArray &content) {
            QFile f(m_dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(content);
        };
        write(QStringLiteral("aur-local"), "foo-bin 1.0-1\n");
        write(QStringLiteral("aur.json"), R"({"type":"multiinfo","results":[{"Name":"foo-bin","Version":"1.1-1"}]})");
        const QString state = m_dir.filePath(QStringLiteral("aur-state.json"));
        const auto check = [&](const QStringList &extra) {
            QProcess p;
            p.start(QStringLiteral(KYU_CHECK_BIN),
                    QStringList{QStringLiteral("--no-pacman"), QStringLiteral("--no-flatpak"), QStringLiteral("--no-news"), QStringLiteral("--state-file"), state,
                                QStringLiteral("--pacman"), pacman, QStringLiteral("--pacman-conf"), m_dir.filePath(QStringLiteral("none")),
                                QStringLiteral("--aur-helper"), helper, QStringLiteral("--aur-rpc"),
                                QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("aur.json"))).toString()}
                        + extra);
            QVERIFY(p.waitForFinished(30000));
        };
        check({});
        QCOMPARE(UpdateState::load(state)->aurPackages.size(), 1);
        // Within six hours the AUR is not asked again (its reply now differs).
        write(QStringLiteral("aur.json"), R"({"type":"multiinfo","results":[]})");
        check({});
        QCOMPARE(UpdateState::load(state)->aurPackages.size(), 1);
        // ...but what was updated meanwhile drops out.
        write(QStringLiteral("aur-local"), "foo-bin 1.1-1\n");
        check({});
        QVERIFY(UpdateState::load(state)->aurPackages.isEmpty());
        // Asked now: the AUR's answer.
        write(QStringLiteral("aur-local"), "foo-bin 1.0-1\n");
        write(QStringLiteral("aur.json"), R"({"type":"multiinfo","results":[{"Name":"foo-bin","Version":"1.2-1"}]})");
        check({QStringLiteral("--aur-now")});
        QCOMPARE(UpdateState::load(state)->aurPackages.value(0).newVersion, QStringLiteral("1.2-1"));
        // Asked by a person (the card's "Check for updates" leaves a mark).
        write(QStringLiteral("aur.json"), R"({"type":"multiinfo","results":[{"Name":"foo-bin","Version":"1.3-1"}]})");
        write(QStringLiteral("aur-now"), "");
        check({});
        QCOMPARE(UpdateState::load(state)->aurPackages.value(0).newVersion, QStringLiteral("1.3-1"));
        QVERIFY(!QFile::exists(m_dir.filePath(QStringLiteral("aur-now"))));
        // No helper: the AUR is left alone.
        check({QStringLiteral("--aur-helper"), QStringLiteral("none")});
        QVERIFY(UpdateState::load(state)->aurPackages.isEmpty());
        QVERIFY(!UpdateState::load(state)->aur.checked);
    }

    void runningCheckIsNotDuplicated()
    {
        QLockFile lock(m_dir.filePath(QStringLiteral("state/check.lock")));
        QVERIFY(lock.tryLock(0));
        const auto before = UpdateState::load(statePath());
        QByteArray err;
        QCOMPARE(run(writeScript(QStringLiteral("checkupdates-ok2"), "echo 'a 1-1 -> 2-1'\n"), &err), 0);
        QVERIFY(err.contains("already running"));
        QCOMPARE(UpdateState::load(statePath()), before);
    }
};

QTEST_GUILESS_MAIN(TestKyuCheck)
#include "test_kyucheck.moc"
