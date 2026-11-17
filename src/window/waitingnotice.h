#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class Activator;
class KNotification;
class QTimer;
class QWidget;

// "The update is waiting for you": while kyu-update waits on a question
// (waiting.h) and this window is not in front, a notification that stays
// until answered, and the taskbar entry asks for attention. Closed when the
// question is answered or the window comes to the front; replaced, never
// stacked, when the question changes. Switchable in System Settings →
// Notifications (kyu.notifyrc, "waitingForYou").
class WaitingNotice : public QObject
{
public:
    WaitingNotice(QWidget *window, Activator *activator);
    ~WaitingNotice() override;

private:
    void look();
    void close();

    QWidget *m_window;
    Activator *m_activator;
    QTimer *m_timer;
    QString m_shown;
#ifdef KYU_HAVE_KNOTIFICATIONS
    QPointer<KNotification> m_notification;
#endif
};
