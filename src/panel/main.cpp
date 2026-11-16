// kyu: the update panel. A viewer: it lists updates, asks kyu-helper to run
// them and shows progress. Closing it never affects a running update; opening
// it again picks the update up from the helper's log.

#include "checkscheduler.h"
#include "flatpakfollower.h"
#include "helperclient.h"
#include "panelcontroller.h"
#include "singleinstance.h"
#include "statewatcher.h"
#include "testdriver.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#ifdef KYU_HAVE_KWINDOWSYSTEM
#include <KWindowSystem>
#endif

#ifdef KYU_PREVIEW
#include "preview/preview.h"
#endif

namespace
{
void chooseStyle()
{
    if (!qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        return;
    }
    // Plasma's style draws controls with the user's widget style, colours
    // and fonts. Fall back to Fusion, which follows the palette.
    const QString kdeStyle = QLibraryInfo::path(QLibraryInfo::QmlImportsPath) + QStringLiteral("/org/kde/desktop/qmldir");
    QQuickStyle::setStyle(QFileInfo::exists(kdeStyle) ? QStringLiteral("org.kde.desktop") : QStringLiteral("Fusion"));
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kyu"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kyu"));
    QApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(KYU_APP_ID));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("update-low")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lists pending updates and applies them through kyu-helper."));
    parser.addHelpOption();
    parser.addVersionOption();
    // Not "--session": QGuiApplication takes "-session <id>" for X11
    // session management and removes it from the arguments.
    QCommandLineOption session(QStringLiteral("session-bus"), QStringLiteral("Use a helper on the session bus (development)."));
    parser.addOption(session);
    QCommandLineOption style(QStringLiteral("style"), QStringLiteral("Look for this run: \"plasma\" or \"terminal\" (not saved)."), QStringLiteral("name"));
    parser.addOption(style);
    TestDriver::addOptions(parser);
#ifdef KYU_PREVIEW
    Preview::addOptions(parser);
#endif
    parser.process(app);

    SingleInstance instance;
    if (!instance.claim()) {
        return 0;
    }

    chooseStyle();

    StateWatcher watcher;
    watcher.reload();
    CheckScheduler scheduler;
    HelperClient helper(parser.isSet(session) ? HelperClient::Bus::Session : HelperClient::Bus::System);
    FlatpakFollower flatpak;
    PanelController panel;
    if (parser.isSet(style)) {
        if (!PanelController::isStyle(parser.value(style))) {
            qWarning("kyu: --style takes \"plasma\" or \"terminal\"");
            return 2;
        }
        panel.overrideStyle(parser.value(style));
    }
    panel.setStateWatcher(&watcher);
    panel.setCheckScheduler(&scheduler);
    panel.setFlatpakFollower(&flatpak);
    panel.setHelper(&helper, parser.isSet(session));
    panel.attach();

    QQmlApplicationEngine engine;
#ifdef KYU_PREVIEW
    Preview::setUp(engine, parser);
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("Panel"), &panel);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QObject::connect(&instance, &SingleInstance::activated, &panel, [&panel, window]([[maybe_unused]] const QString &token) {
#ifndef KYU_HAVE_KWINDOWSYSTEM
        // Qt's Wayland plugin uses this for the next requestActivate().
        if (!token.isEmpty()) {
            qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
        }
#endif
        Q_EMIT panel.activateRequested(); // show, raise, requestActivate (Main.qml)
#ifdef KYU_HAVE_KWINDOWSYSTEM
        // On Wayland, focus needs the tray click's activation token.
        if (window && !token.isEmpty()) {
            KWindowSystem::setCurrentXdgActivationToken(token);
            KWindowSystem::activateWindow(window);
        }
#else
        Q_UNUSED(window)
#endif
    });
#ifdef KYU_PREVIEW
    Preview::run(engine);
#endif
    TestDriver::run(engine, parser, panel, parser.isSet(session));
    return app.exec();
}
