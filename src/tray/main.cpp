// kyu-tray: tray icon with the number of pending updates. Reads the state that
// kyu-check writes; never runs as root and never changes the system.

#include "checkscheduler.h"
#include "statewatcher.h"
#include "trayicon.h"

#include <QApplication>
#include <QFileInfo>
#include <QProcess>

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
    QObject::connect(&tray, &TrayIcon::panelRequested, &tray, [] {
        // The panel is its own process (so it can close or crash without
        // touching the tray) and is single-instance: a second start raises it.
        QString panel = QApplication::applicationDirPath() + QStringLiteral("/kyu");
        if (!QFileInfo(panel).isExecutable()) {
            panel = QFileInfo(QStringLiteral(KYU_PANEL_BINARY)).isExecutable() ? QStringLiteral(KYU_PANEL_BINARY) : QStringLiteral("kyu");
        }
        if (!QProcess::startDetached(panel, {})) {
            qWarning("kyu-tray: could not start %s", qPrintable(panel));
        }
    });

    watcher.reload();
    scheduler.start();
    return app.exec();
}
