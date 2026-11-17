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
    // paru's and yay's menus: the question two lines up, an empty "==> ".
    QCOMPARE(waitingPrompt(":: Packages to cleanBuild?\r\n:: [N]one [A]ll [Ab]ort [I]nstalled [No]tInstalled or (1 2 3, 1-3, ^4)\r\n\x1b[1;32m==> \x1b[0m"),
             QStringLiteral("Packages to cleanBuild?"));
    QCOMPARE(waitingPrompt("==> "), QStringLiteral("An answer is needed in the Kyu window."));
    // A pager: a PKGBUILD to review.
    QCOMPARE(waitingPrompt("pkgname=foo\r\n:"), QStringLiteral("Something to read in the Kyu window."));
    QCOMPARE(waitingPrompt("source=()\r\n(END)"), QStringLiteral("Something to read in the Kyu window."));
    QCOMPARE(waitingPrompt("lines 1-40"), QStringLiteral("Something to read in the Kyu window."));
    // flatpak's question; a charset switch after a prompt (tput sgr0).
    QCOMPARE(waitingPrompt("Do you want to proceed? [Y/n]: "), QStringLiteral("Do you want to proceed? [Y/n]:"));
    QCOMPARE(waitingPrompt("\x1b[1m==> Proceed with installation? [Y/n]\x1b(B\x1b[m "), QStringLiteral("Proceed with installation? [Y/n]"));
    // pacman's provider choice: the question above the number.
    QCOMPARE(waitingPrompt(":: There are 2 providers available for jack:\r\n:: Repository extra\r\n   1) jack2  2) pipewire-jack\r\n\r\nEnter a number (default=1): "),
             QStringLiteral("There are 2 providers available for jack:"));
    // A fingerprint reader.
    QCOMPARE(waitingPrompt("Place your finger on the fingerprint reader"), QStringLiteral("Your confirmation is needed."));
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
