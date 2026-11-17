#include "waiting.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Kyu;

class TestWaiting : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void prompts();
    void file();
};

void TestWaiting::prompts()
{
    // pacman's question, with its colours and the lines before it.
    QCOMPARE(waitingPrompt("Total Download Size:  412.00 MiB\r\n\r\n\x1b[1;34m::\x1b[0;1m Proceed with installation? [Y/n] \x1b[0m"),
             QStringLiteral("Proceed with installation? [Y/n]"));
    QCOMPARE(waitingPrompt(":: Fortfahren mit der Installation? [J/n] "), QStringLiteral("Fortfahren mit der Installation? [J/n]"));
    QCOMPARE(waitingPrompt(":: Replace foo with extra/bar? [Y/n] "), QStringLiteral("Replace foo with extra/bar? [Y/n]"));
    QCOMPARE(waitingPrompt("Enter a selection (default=all): "), QStringLiteral("Enter a selection (default=all):"));
    // Passwords, in any language: the prompt itself is not repeated.
    QCOMPARE(waitingPrompt("[sudo] password for someone: "), QStringLiteral("Your password is needed."));
    QCOMPARE(waitingPrompt("[sudo] Passwort für someone: "), QStringLiteral("Your password is needed."));
    QCOMPARE(waitingPrompt("doas (someone@host) password: "), QStringLiteral("Your password is needed."));
    // Kyu's own question.
    QCOMPARE(waitingPrompt("Continue? [y/N] "), QStringLiteral("Continue? [y/N]"));
    // Not questions: a progress bar (redrawn with \r), output that ended
    // its line, nothing.
    QVERIFY(waitingPrompt(" mesa  12.3 MiB  5.00 MiB/s 00:02 [######------]  60%\r mesa  12.3 MiB  5.00 MiB/s 00:01 [########----]  80%").isEmpty());
    QVERIFY(waitingPrompt("(2/2) checking keys in keyring    [##########] 100%").isEmpty());
    QVERIFY(waitingPrompt(":: Proceed with installation? [Y/n] y\r\n").isEmpty());
    QVERIFY(waitingPrompt("==> Running build hook: [autodetect]").isEmpty());
    QVERIFY(waitingPrompt("").isEmpty());
}

void TestWaiting::file()
{
    QTemporaryDir dir;
    qputenv("XDG_STATE_HOME", dir.path().toUtf8());
    QVERIFY(waiting().isEmpty());
    setWaiting(QStringLiteral("Your password is needed."));
    QCOMPARE(waiting(), QStringLiteral("Your password is needed."));
    setWaiting(QString());
    QVERIFY(waiting().isEmpty());
}

QTEST_GUILESS_MAIN(TestWaiting)
#include "test_waiting.moc"
