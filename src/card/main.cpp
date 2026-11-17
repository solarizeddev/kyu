// kyu: the card the tray opens. What is going on (updates, news to read
// first, a restart to finish, a settings file to merge, a failed update) and
// the one thing to do next, which opens the update in Kyu's window.

#include "cardwindow.h"
#include "checkscheduler.h"
#include "singleinstance.h"
#include "statewatcher.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>

#ifdef KYU_HAVE_KWINDOWSYSTEM
#include <KWindowSystem>
#endif

namespace
{
// The card's states, for screenshots (--demo NAME).
std::optional<Kyu::CardView> demo(const QString &name)
{
    using A = Kyu::CardView::Action;
    using K = Kyu::CardView::Kind;
    Kyu::CardView c;
    auto set = [&c](K kind, const QString &stamp, const QString &heading, const QString &detail, A action, const QString &actionText, bool primary) {
        c.kind = kind;
        c.stamp = stamp;
        c.heading = heading;
        c.detail = detail;
        c.action = action;
        c.actionText = actionText;
        c.primary = primary;
    };
    if (name == QLatin1String("updates")) {
        set(K::Updates, QStringLiteral("checked 14:02"), QStringLiteral("8 updates"), QStringLiteral("6 packages · 2 Flatpaks · 412 MB · restart after"),
            A::Update, QStringLiteral("Update"), true);
        c.linkText = QStringLiteral("Read first: Stable Update 2026-11-14");
        c.linkUrl = QStringLiteral("https://forum.manjaro.org/c/announcements/stable-updates/12");
    } else if (name == QLatin1String("flatpaks")) {
        set(K::Updates, QStringLiteral("checked 14:02"), QStringLiteral("2 updates"), QStringLiteral("2 Flatpaks · no password needed"), A::Update,
            QStringLiteral("Update"), true);
    } else if (name == QLatin1String("uptodate")) {
        set(K::UpToDate, QStringLiteral("checked 14:02"), QStringLiteral("Up to date"), {}, A::Check, QStringLiteral("Check for updates"), false);
    } else if (name == QLatin1String("checking")) {
        set(K::Checking, {}, QStringLiteral("Checking…"), {}, A::None, {}, false);
    } else if (name == QLatin1String("checkfailed")) {
        set(K::CheckFailed, {}, QStringLiteral("Could not check"), QStringLiteral("pacman check failed at 14:02: could not resolve host; list from yesterday 20:02"),
            A::Check, QStringLiteral("Check for updates"), false);
    } else if (name == QLatin1String("updating")) {
        set(K::Updating, {}, QStringLiteral("Updating…"), QStringLiteral("In the Kyu window."), A::None, {}, false);
    } else if (name == QLatin1String("restart")) {
        set(K::Restart, QStringLiteral("updated 14:12"), QStringLiteral("Restart to finish"), QStringLiteral("New kernel 6.18.22-1-MANJARO"), A::Restart,
            QStringLiteral("Restart"), true);
        c.mergeText = QStringLiteral("1 settings file changed: /etc/sddm.conf");
    } else if (name == QLatin1String("settings")) {
        set(K::Settings, QStringLiteral("updated 14:12"), QStringLiteral("1 settings file changed"), QStringLiteral("/etc/sddm.conf has a new version"),
            A::Merge, QStringLiteral("Merge…"), true);
    } else if (name == QLatin1String("failed")) {
        set(K::Failed, QStringLiteral("tried 14:12"), QStringLiteral("The update did not finish"), {}, A::Update, QStringLiteral("Update again"), true);
        c.failureKind = QStringLiteral("keyring");
    } else {
        return std::nullopt;
    }
    return c;
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
    parser.setApplicationDescription(QStringLiteral("What is going on with updates, and the one thing to do next."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demoOption(QStringLiteral("demo"),
                                  QStringLiteral("Show a sample state (updates, flatpaks, uptodate, checking, checkfailed, updating, restart, "
                                                 "settings, failed), for screenshots."),
                                  QStringLiteral("name"));
    QCommandLineOption shot(QStringLiteral("screenshot"), QStringLiteral("Save the card to this PNG file and quit."), QStringLiteral("file"));
    parser.addOptions({demoOption, shot});
    parser.process(app);

    std::optional<Kyu::CardView> fixed;
    if (parser.isSet(demoOption)) {
        fixed = demo(parser.value(demoOption));
        if (!fixed) {
            qWarning("kyu: unknown --demo state");
            return 2;
        }
    }

    SingleInstance instance;
    if (!fixed && !instance.claim()) {
        return 0;
    }

    StateWatcher watcher;
    watcher.reload();
    CheckScheduler scheduler;
    CardWindow card(fixed ? nullptr : &watcher, &scheduler);
    if (fixed) {
        card.setFixedView(*fixed);
    }
    card.show();

    if (parser.isSet(shot)) {
        const QString file = parser.value(shot);
        QTimer::singleShot(300, &card, [&card, file]() {
            card.grab().save(file);
            QApplication::quit();
        });
        return app.exec();
    }

    // A second start (another tray click) brings this card forward.
    QObject::connect(&instance, &SingleInstance::activated, &card, [&card]([[maybe_unused]] const QString &token) {
        card.show();
        card.raise();
#ifdef KYU_HAVE_KWINDOWSYSTEM
        // On Wayland, focus needs the tray click's activation token.
        if (!token.isEmpty()) {
            KWindowSystem::setCurrentXdgActivationToken(token);
        }
        KWindowSystem::activateWindow(card.windowHandle());
#else
        card.activateWindow();
#endif
    });
    return app.exec();
}
