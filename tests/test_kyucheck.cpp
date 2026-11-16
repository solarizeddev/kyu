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
                {QStringLiteral("--no-flatpak"),
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
                             "elif [ \"$1\" = -Sup ] && [ \"$4 $5\" = '--print-format %n %s' ]; then\n"
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
        log.write("[2026-11-10T20:30:00+0000] [PACMAN] starting full system upgrade\n");
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
                {QStringLiteral("--no-flatpak"), QStringLiteral("--state-file"), statePath(), QStringLiteral("--db-path"),
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
