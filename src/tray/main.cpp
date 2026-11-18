// kyu-tray: tray icon with the number of pending updates. Reads the state that
// kyu-check writes; never runs as root and never changes the system.

#include "checkscheduler.h"
#include "i18n.h"
#include "statewatcher.h"
#include "systemwatcher.h"
#include "trayicon.h"

#include <QApplication>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Kyu::loadTranslations();
    QApplication::setApplicationName(QStringLiteral("kyu-tray"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kyu"));
    QApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(KYU_APP_ID ".tray"));
    // The tray has no windows; closing the card must not quit it.
    QApplication::setQuitOnLastWindowClosed(false);

    StateWatcher watcher;
    CheckScheduler scheduler;
    TrayIcon tray(&watcher);
    QObject::connect(&tray, &TrayIcon::checkRequested, &scheduler, &CheckScheduler::checkNowAsked);
    // Check again when packages changed outside Kyu (pamac, plain pacman),
    // after sleep, and when the network is back after a failed check.
    SystemWatcher system;
    system.watchSystemBus();
    QObject::connect(&system, &SystemWatcher::packagesChanged, &scheduler, &CheckScheduler::checkNow);
    // After waking (or the network coming back), the first check may have
    // run before the network was up, or still be running: check, and once
    // more two minutes later if the result is still a failure.
    auto failed = [&watcher]() {
        return !watcher.loadError().isEmpty() || (watcher.state() && watcher.state()->hasError());
    };
    auto checkUntilFine = [&scheduler, failed]() {
        scheduler.checkNow();
        QTimer::singleShot(120000, &scheduler, [&scheduler, failed]() {
            if (failed()) {
                scheduler.checkNow();
            }
        });
    };
    QObject::connect(&system, &SystemWatcher::resumed, &scheduler, checkUntilFine);
    QObject::connect(&system, &SystemWatcher::networkUp, &scheduler, [failed, checkUntilFine]() {
        if (failed()) {
            checkUntilFine();
        }
    });
    QObject::connect(&tray, &TrayIcon::cardRequested, &tray, [](const QString &token) {
        // The card is its own process (so it can close or crash without
        // touching the tray) and is single-instance: a second start raises it.
        QString card = QApplication::applicationDirPath() + QStringLiteral("/kyu");
        if (!QFileInfo(card).isExecutable()) {
            card = QFileInfo(QStringLiteral(KYU_CARD_BINARY)).isExecutable() ? QStringLiteral(KYU_CARD_BINARY) : QStringLiteral("kyu");
        }
        // The click's activation token lets the card take focus on Wayland
        // (a new card reads it from the environment; a running one gets it
        // through its Activate call).
        QProcess process;
        process.setProgram(card);
        if (!token.isEmpty()) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);
            process.setProcessEnvironment(env);
        }
        if (!process.startDetached()) {
            qWarning("kyu-tray: could not start %s", qPrintable(card));
        }
    });

    watcher.reload();
    scheduler.start();
    return app.exec();
}
