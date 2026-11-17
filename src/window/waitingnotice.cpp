#include "waitingnotice.h"

#include "activator.h"
#include "waiting.h"

#include <QApplication>
#include <QTimer>
#include <QWidget>

#ifdef KYU_HAVE_KNOTIFICATIONS
#include <KNotification>
#endif

WaitingNotice::WaitingNotice(QWidget *window, Activator *activator)
    : QObject(window)
    , m_window(window)
    , m_activator(activator)
    , m_timer(new QTimer(this))
{
    // kyu-update notes a question after 1.5 s of silence; looked at every
    // second (a file that comes and goes; cheap).
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &WaitingNotice::look);
    m_timer->start();
}

WaitingNotice::~WaitingNotice()
{
    close();
}

void WaitingNotice::look()
{
    const QString text = Kyu::waiting();
    if (text.isEmpty() || m_window->isActiveWindow()) {
        close();
        return;
    }
    if (text == m_shown) {
        return;
    }
    close();
    m_shown = text;
    // The taskbar entry too (Plasma highlights it until the window is shown).
    QApplication::alert(m_window);
#ifdef KYU_HAVE_KNOTIFICATIONS
    auto *notification = new KNotification(QStringLiteral("waitingForYou"), KNotification::Persistent);
    notification->setComponentName(QStringLiteral("kyu"));
    notification->setIconName(QStringLiteral("system-software-update"));
    notification->setTitle(QStringLiteral("The update is waiting for you"));
    notification->setText(text);
    notification->setUrgency(KNotification::HighUrgency);
    const auto show = [this, notification]() {
        m_activator->Activate(notification->xdgActivationToken());
    };
    connect(notification->addDefaultAction(QStringLiteral("Show")), &KNotificationAction::activated, this, show);
    connect(notification->addAction(QStringLiteral("Show")), &KNotificationAction::activated, this, show);
    m_notification = notification;
    notification->sendEvent();
#endif
}

void WaitingNotice::close()
{
    m_shown.clear();
#ifdef KYU_HAVE_KNOTIFICATIONS
    if (m_notification) {
        m_notification->close();
    }
    m_notification = nullptr;
#endif
}
