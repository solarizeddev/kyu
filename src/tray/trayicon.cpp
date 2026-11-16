#include "trayicon.h"

#include "badge.h"
#include "statewatcher.h"
#include "statustext.h"

#include <QAction>
#include <QApplication>
#include <QMenu>

#ifdef KYU_HAVE_KSNI
#include <KStatusNotifierItem>
#else
#include <QSystemTrayIcon>
#endif

namespace
{
// Theme icon names only; the icon theme decides how they look.
const QString IconNone = QStringLiteral("update-none");
const QString IconUpdates = QStringLiteral("update-low");
const QString IconProblem = QStringLiteral("emblem-warning");
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
    const int total = state ? state->total() : 0;
    [[maybe_unused]] const bool problem = !m_watcher->loadError().isEmpty() || (state && state->hasError());

    m_checkAction->setEnabled(!checking);

    const QString iconName = total > 0 ? IconUpdates : IconNone;
    const QStringList lines = Kyu::statusLines(state, checking, m_watcher->loadError(), QDateTime::currentDateTime());

#ifdef KYU_HAVE_KSNI
    // The icon by name, so Plasma colours it for the panel (which may be dark
    // while applications are light); the count or a problem as an overlay.
    m_item->setIconByName(iconName);
    if (problem) {
        m_item->setOverlayIconByPixmap(QIcon());
        m_item->setOverlayIconByName(IconProblem);
    } else {
        m_item->setOverlayIconByName(QString());
        m_item->setOverlayIconByPixmap(countOverlay(total, QApplication::palette()));
    }
    m_item->setToolTipIconByName(iconName);
    QStringList escaped;
    for (const QString &line : lines) {
        escaped.append(line.toHtmlEscaped());
    }
    m_item->setToolTipSubTitle(escaped.join(QStringLiteral("<br/>")));
    // Passive moves the icon into the tray's hidden section: nothing to do.
    const bool idle = total == 0 && !problem && !checking;
    m_item->setStatus(idle ? KStatusNotifierItem::Passive : KStatusNotifierItem::Active);
#else
    m_item->setIcon(badgedIcon(QIcon::fromTheme(iconName), total, QApplication::palette()));
    m_item->setToolTip(lines.join(QLatin1Char('\n')));
#endif
}
