// kyu-tray: tray icon with the number of pending updates. Reads the state that
// kyu-check writes; never runs as root and never changes the system.

#include "checkscheduler.h"
#include "statewatcher.h"
#include "trayicon.h"

#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kyu-tray"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kyu"));
    QApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(KYU_APP_ID ".tray"));
    // The tray has no windows; closing a future panel must not quit it.
    QApplication::setQuitOnLastWindowClosed(false);

    StateWatcher watcher;
    CheckScheduler scheduler;
    TrayIcon tray(&watcher);
    QObject::connect(&tray, &TrayIcon::checkRequested, &scheduler, &CheckScheduler::checkNow);

    watcher.reload();
    scheduler.start();
    return app.exec();
}
