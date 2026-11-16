#include "trayicon.h"

#include "aftercare.h"
#include "badge.h"
#include "paths.h"
#include "statewatcher.h"
#include "statustext.h"
#include "traystate.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <csignal>

#ifdef KYU_HAVE_KSNI
#include <KStatusNotifierItem>
#else
#include <QSystemTrayIcon>
#endif

namespace
{
// The "Updating" icon's frames, installed into the hicolor icon theme as
// kyu-updating-0 … kyu-updating-23 (1.5 s per turn).
constexpr int SpinFrames = 24;
constexpr int SpinInterval = 62; // ms

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

QString bootId()
{
    QFile file(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

// A kyu-update holds its lock (read only: taking it would make a starting
// kyu-update think another one runs).
bool updateRunning()
{
    QLockFile lock(Kyu::Paths::stateDir() + QStringLiteral("/update.lock"));
    qint64 pid = 0;
    QString host, app;
    return lock.getLockInfo(&pid, &host, &app) && pid > 0 && ::kill(pid_t(pid), 0) == 0;
}

// Plasma's "Animation speed: instant" (and reduced motion) is a factor of 0.
bool animationsEnabled()
{
    const QString kdeglobals = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QStringLiteral("kdeglobals"));
    if (kdeglobals.isEmpty()) {
        return true;
    }
    QSettings settings(kdeglobals, QSettings::IniFormat);
    return settings.value(QStringLiteral("KDE/AnimationDurationFactor"), 1.0).toDouble() > 0.0;
}
}

TrayIcon::TrayIcon(StateWatcher *watcher, QObject *parent)
    : QObject(parent)
    , m_watcher(watcher)
    , m_menu(new QMenu)
{
    auto *show = m_menu->addAction(QIcon::fromTheme(QStringLiteral("system-software-update")), QStringLiteral("Show updates"));
    connect(show, &QAction::triggered, this, [this] {
        Q_EMIT panelRequested(QString());
    });
    m_checkAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Check for updates now"));
    connect(m_checkAction, &QAction::triggered, this, &TrayIcon::checkRequested);

#ifdef KYU_HAVE_KSNI
    m_item = new KStatusNotifierItem(QStringLiteral("kyu"), this);
    m_item->setCategory(KStatusNotifierItem::SystemServices);
    m_item->setTitle(QStringLiteral("Kyu"));
    m_item->setToolTipTitle(QStringLiteral("Updates"));
    m_item->setContextMenu(m_menu); // takes ownership; adds Quit
    // No associated window: a click asks for the panel, a separate process.
    connect(m_item, &KStatusNotifierItem::activateRequested, this, [this] {
        Q_EMIT panelRequested(m_item->providedToken());
    });
#else
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Quit"), qApp, &QCoreApplication::quit);
    m_item = new QSystemTrayIcon(this);
    m_item->setContextMenu(m_menu);
    connect(this, &QObject::destroyed, m_menu, &QObject::deleteLater);
    connect(m_item, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            Q_EMIT panelRequested(QString());
        }
    });
#endif

    connect(m_watcher, &StateWatcher::changed, this, &TrayIcon::refresh);
    // kyu-update's lock and last-run.json, and the running kernel: looked at
    // every few seconds (cheap; no watcher for a lock that comes and goes).
    m_poll = new QTimer(this);
    m_poll->setInterval(3000);
    connect(m_poll, &QTimer::timeout, this, &TrayIcon::refresh);
    m_poll->start();
    m_spin = new QTimer(this);
    m_spin->setInterval(SpinInterval);
    connect(m_spin, &QTimer::timeout, this, &TrayIcon::spin);
    // Redraw the badge when the colour scheme changes.
    qApp->installEventFilter(this);
    refresh();
#ifndef KYU_HAVE_KSNI
    m_item->show();
#endif
}

bool TrayIcon::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange) {
        refresh();
    }
    return QObject::eventFilter(watched, event);
}

void TrayIcon::refresh()
{
    const auto &state = m_watcher->state();
    const bool checking = m_watcher->checking();
    m_checkAction->setEnabled(!checking);

    Kyu::TrayInput in;
    in.state = state;
    in.checking = checking;
    in.loadError = m_watcher->loadError();
    in.lastRun = readJson(Kyu::Paths::stateDir() + QStringLiteral("/last-run.json"));
    const QString boot = bootId();
    in.sameBoot = !boot.isEmpty() && in.lastRun.value(QStringLiteral("bootId")).toString() == boot;
    in.kernelReplaced = Kyu::kernelState().replaced;
    in.updating = updateRunning();
    in.now = QDateTime::currentDateTime();
    const Kyu::TrayView view = Kyu::trayView(in);

#ifdef KYU_HAVE_KSNI
    // Icons by name, so Plasma colours them for the panel (which may be dark
    // while applications are light). No overlay: the icon itself has the mark.
    m_item->setOverlayIconByName(QString());
    m_item->setOverlayIconByPixmap(QIcon());
    const bool spinning = view.mark == Kyu::TrayMark::Updating && QIcon::hasThemeIcon(QStringLiteral("kyu-updating-0"));
    if (spinning) {
        if (animationsEnabled()) {
            if (!m_spin->isActive()) {
                m_frame = 0;
                m_spin->start();
            }
        } else {
            m_spin->stop();
            m_item->setIconByName(QStringLiteral("kyu-updating-0"));
        }
    } else {
        m_spin->stop();
        QString name = view.iconName;
        if (view.mark == Kyu::TrayMark::Updating) {
            name = QStringLiteral("update-low"); // frames not installed (a build tree)
        } else if (!QIcon::hasThemeIcon(name) && name == QLatin1String("system-reboot-update")) {
            name = QStringLiteral("system-reboot");
        }
        m_item->setIconByName(name);
    }
    m_item->setToolTipIconByName(view.mark == Kyu::TrayMark::Updating ? QStringLiteral("update-low") : view.iconName);
    m_item->setToolTipTitle(QStringLiteral("Kyu"));
    QStringList escaped;
    for (const QString &line : view.tooltip.split(QLatin1Char('\n'))) {
        escaped.append(line.toHtmlEscaped());
    }
    m_item->setToolTipSubTitle(escaped.join(QStringLiteral("<br/>")));
    // Passive moves the icon into the tray's hidden section: nothing to do.
    m_item->setStatus(view.passive ? KStatusNotifierItem::Passive : KStatusNotifierItem::Active);
#else
    const int total = state ? state->total() : 0;
    m_item->setIcon(badgedIcon(QIcon::fromTheme(view.iconName.isEmpty() ? QStringLiteral("update-low") : view.iconName), total, QApplication::palette()));
    m_item->setToolTip(view.tooltip);
#endif
}

void TrayIcon::spin()
{
#ifdef KYU_HAVE_KSNI
    m_item->setIconByName(QStringLiteral("kyu-updating-%1").arg(m_frame));
#endif
    m_frame = (m_frame + 1) % SpinFrames;
}
