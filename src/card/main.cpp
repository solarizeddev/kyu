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
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#ifdef KYU_HAVE_KWINDOWSYSTEM
#include <KWindowSystem>
#endif
#ifdef KYU_HAVE_KICONTHEMES
#include <KIconTheme>
#endif

namespace
{
// The card's states, for screenshots (--demo NAME): made from a sample
// system through the same cardView() as the real card.
std::optional<Kyu::CardView> demo(const QString &name)
{
    using namespace Kyu;
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime checked = QDateTime(now.date(), QTime(14, 2));
    const QDateTime updated = QDateTime(now.date(), QTime(14, 12));
    TrayInput in;
    in.now = QDateTime(now.date(), QTime(14, 20));
    UpdateState s;
    s.checkedAt = checked;
    for (SourceStatus *status : {&s.pacman, &s.flatpak, &s.aur}) {
        status->checked = status->ok = true;
        status->checkedAt = status->lastSuccessAt = checked;
    }
    const auto package = [](const char *n, const char *repo, const char *from, const char *to, qint64 mb, bool restart = false) {
        return PackageUpdate{QString::fromLatin1(n), QString::fromLatin1(repo), QString::fromLatin1(from), QString::fromLatin1(to), mb * 1024 * 1024, restart};
    };
    const QList<PackageUpdate> packages{package("linux618", "core", "6.18.20-1", "6.18.22-1", 140, true), package("mesa", "extra", "1:26.2.1-1", "1:26.2.3-1", 60),
                                        package("plasma-workspace", "extra", "6.7.4-1", "6.7.5-1", 30), package("kwin", "extra", "6.7.4-1", "6.7.5-1", 12),
                                        package("firefox", "extra", "145.0-1", "145.0.1-1", 80), package("python", "core", "3.14.0-1", "3.14.1-1", 90)};
    const QList<FlatpakUpdate> flatpaks{{QStringLiteral("org.kde.krita"), QStringLiteral("app"), QStringLiteral("stable"), QStringLiteral("flathub"), QStringLiteral("system")},
                                        {QStringLiteral("org.freedesktop.Platform"), QStringLiteral("runtime"), QStringLiteral("25.08"), QStringLiteral("flathub"),
                                         QStringLiteral("system")}};
    const QJsonObject lastRun{{QStringLiteral("time"), updated.toUTC().toString(Qt::ISODate)}, {QStringLiteral("pacmanExit"), 0}};
    if (name == QLatin1String("updates") || name == QLatin1String("stale")) {
        s.packages = packages;
        s.aurPackages = {package("visual-studio-code-bin", "aur", "1.106.0-1", "1.106.2-1", 0)};
        s.aurPackages.first().size = -1;
        s.flatpaks = flatpaks;
        s.news.items = {{QStringLiteral("Stable Update 2026-11-14"), QStringLiteral("https://forum.manjaro.org/c/announcements/stable-updates/12"), checked}};
        if (name == QLatin1String("stale")) {
            s.news.items.clear();
            s.pacman.ok = false;
            s.pacman.error = QStringLiteral("error: failed retrieving file 'core.db' from mirror.example.org : Could not resolve host: mirror.example.org");
            s.pacman.lastSuccessAt = checked.addDays(-1).addSecs(6 * 3600);
        }
    } else if (name == QLatin1String("flatpaks")) {
        s.flatpaks = flatpaks;
    } else if (name == QLatin1String("uptodate")) {
        in.lastRun = lastRun;
        in.lastRun.insert(QStringLiteral("time"), checked.addDays(-1).addSecs(6 * 3600 + 13 * 60).toUTC().toString(Qt::ISODate));
    } else if (name == QLatin1String("checking")) {
        in.checking = true;
        in.state = std::nullopt;
        return cardView(in);
    } else if (name == QLatin1String("checkfailed")) {
        s.pacman.ok = false;
        s.pacman.error = QStringLiteral("error: failed retrieving file 'core.db' from mirror.example.org : Could not resolve host: mirror.example.org");
        s.pacman.lastSuccessAt = checked.addDays(-1).addSecs(6 * 3600);
    } else if (name == QLatin1String("updating") || name == QLatin1String("merging")) {
        in.updating = true;
        in.activity = name == QLatin1String("merging") ? QStringLiteral("merge") : QStringLiteral("update");
    } else if (name == QLatin1String("restart")) {
        in.lastRun = lastRun;
        in.lastRun.insert(QStringLiteral("restartRequired"), true);
        in.lastRun.insert(QStringLiteral("restartReason"), QStringLiteral("new kernel 6.18.22-1-MANJARO"));
        in.sameBoot = true;
        in.kernelReplaced = true;
        in.configs = {QStringLiteral("/etc/sddm.conf.pacnew")};
    } else if (name == QLatin1String("settings")) {
        in.lastRun = lastRun;
        in.configs = {QStringLiteral("/etc/sddm.conf.pacnew")};
    } else if (name == QLatin1String("failed")) {
        in.lastRun = lastRun;
        in.lastRun.insert(QStringLiteral("pacmanExit"), 1);
        in.lastRun.insert(QStringLiteral("failure"), QStringLiteral("keyring"));
        in.lastRun.insert(QStringLiteral("failureLines"),
                          QJsonArray{QStringLiteral("error: mesa: signature from \"Someone <s@example.org>\" is unknown trust"),
                                     QStringLiteral("error: failed to commit transaction (invalid or corrupted package (PGP signature))"),
                                     QStringLiteral("Errors occurred, no packages were upgraded.")});
        s.packages = packages;
    } else {
        return std::nullopt;
    }
    in.state = s;
    return cardView(in);
}
}

int main(int argc, char **argv)
{
#ifdef KYU_HAVE_KICONTHEMES
    // Before the application: KDE's icon engine, so icons take the colour
    // scheme's colours in any session.
    KIconTheme::initTheme();
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kyu"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kyu"));
    QApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(KYU_APP_ID));
    // Kyu's icon, as in its menu entry and on Kyu's update window.
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("system-software-update")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("What is going on with updates, and the one thing to do next."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demoOption(QStringLiteral("demo"),
                                  QStringLiteral("Show a sample state (updates, stale, flatpaks, uptodate, checking, checkfailed, updating, merging, "
                                                 "restart, settings, failed), for screenshots."),
                                  QStringLiteral("name"));
    QCommandLineOption shot(QStringLiteral("screenshot"), QStringLiteral("Save the card to this PNG file and quit."), QStringLiteral("file"));
    QCommandLineOption detailsOption(QStringLiteral("details"), QStringLiteral("With --demo: Details open."));
    parser.addOptions({demoOption, shot, detailsOption});
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
        card.showDetails(parser.isSet(detailsOption));
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
