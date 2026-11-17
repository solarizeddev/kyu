#include "trayicon.h"

#include "aftercare.h"
#include "badge.h"
#include "news.h"
#include "paths.h"
#include "statewatcher.h"
#include "statustext.h"
#include "traystate.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>


#ifdef KYU_HAVE_KNOTIFICATIONS
#include <KNotification>
#endif

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

// Plasma's "Animation speed: instant" (and reduced motion) is a factor of 0.
[[maybe_unused]] bool animationsEnabled()
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
        Q_EMIT cardRequested(QString());
    });
    m_checkAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), QStringLiteral("Check for updates now"));
    connect(m_checkAction, &QAction::triggered, this, &TrayIcon::checkRequested);

#ifdef KYU_HAVE_KSNI
    m_item = new KStatusNotifierItem(QStringLiteral("kyu"), this);
    m_item->setCategory(KStatusNotifierItem::SystemServices);
    m_item->setTitle(QStringLiteral("Kyu"));
    m_item->setToolTipTitle(QStringLiteral("Updates"));
    m_item->setContextMenu(m_menu); // takes ownership; adds Quit
    // No associated window: a click asks for the card, a separate process.
    connect(m_item, &KStatusNotifierItem::activateRequested, this, [this] {
        Q_EMIT cardRequested(m_item->providedToken());
    });
#else
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Quit"), qApp, &QCoreApplication::quit);
    m_item = new QSystemTrayIcon(this);
    m_item->setContextMenu(m_menu);
    connect(this, &QObject::destroyed, m_menu, &QObject::deleteLater);
    connect(m_item, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            Q_EMIT cardRequested(QString());
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

    const Kyu::TrayInput in = Kyu::currentTrayInput(state, checking, m_watcher->loadError());
    const Kyu::TrayView view = Kyu::trayView(in);
    notifyNews(view.newsKey, view.newsTitle, view.newsLink, view.newsSource);

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

// "News": once per announcement, whether updates are pending or not; it
// stays until dismissed (it may need action before the next update), and
// goes when the announcement is read (in kyu-update, the card, or here).
// Switchable in System Settings → Notifications (kyu.notifyrc, "news").
void TrayIcon::notifyNews(const QString &key, const QString &title, const QString &link, const QString &source)
{
#ifdef KYU_HAVE_KNOTIFICATIONS
    if (m_news && key != m_newsKey) {
        m_news->close();
    }
#endif
    if (key.isEmpty() || key == m_newsKey) {
        if (key.isEmpty()) {
            m_newsKey.clear();
        }
        return;
    }
    m_newsKey = key;
    // Remembered apart from "read": a notification once, even across
    // restarts of the tray, for an announcement still unread.
    const QString notified = Kyu::Paths::stateDir() + QStringLiteral("/news-notified.json");
    if (Kyu::seenNews(notified).contains(key)) {
        return;
    }
    Kyu::markNewsSeen({key}, notified);
#ifdef KYU_HAVE_KNOTIFICATIONS
    auto *notification = new KNotification(QStringLiteral("news"), KNotification::Persistent);
    notification->setComponentName(QStringLiteral("kyu"));
    notification->setIconName(QStringLiteral("update-medium"));
    notification->setTitle(source.isEmpty() ? QStringLiteral("News to read before updating") : source);
    notification->setText(title.toHtmlEscaped());
    if (!link.isEmpty()) {
        connect(notification->addAction(QStringLiteral("Read")), &KNotificationAction::activated, this, [key, link]() {
            Kyu::markNewsSeen({key});
            QDesktopServices::openUrl(QUrl(link));
        });
    }
    connect(notification->addDefaultAction(QStringLiteral("Show updates")), &KNotificationAction::activated, this, [this, notification]() {
        Q_EMIT cardRequested(notification->xdgActivationToken());
    });
    m_news = notification;
    notification->sendEvent();
#else
    Q_UNUSED(title)
    Q_UNUSED(link)
    Q_UNUSED(source)
#endif
}
