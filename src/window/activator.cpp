#include "activator.h"

#include <QDBusConnection>
#include <QWindow>

#ifdef KYU_HAVE_KWINDOWSYSTEM
#include <KWindowSystem>
#endif

Activator::Activator(QWidget *window)
    : QObject(window)
    , m_window(window)
{
}

bool Activator::publish()
{
    auto bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("io.github.solarizeddev.kyu.Window");
    if (!bus.isConnected() || !bus.registerService(service)) {
        return false;
    }
    return bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
}

void Activator::Activate(const QString &token)
{
    if (!m_window) {
        return;
    }
    // Out of the taskbar if minimized; maximized stays maximized.
    m_window->setWindowState(m_window->windowState() & ~Qt::WindowMinimized);
    m_window->show();
    m_window->raise();
#ifdef KYU_HAVE_KWINDOWSYSTEM
    if (!token.isEmpty()) {
        KWindowSystem::setCurrentXdgActivationToken(token);
    }
    KWindowSystem::activateWindow(m_window->windowHandle());
#else
    Q_UNUSED(token)
    m_window->activateWindow();
#endif
}
