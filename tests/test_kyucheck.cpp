// Runs the kyu-check binary against fake checkupdates/pacman scripts.

#include "updatestate.h"

#include <QFile>
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
