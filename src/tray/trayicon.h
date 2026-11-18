#pragma once

#include "notices.h"

#include <QObject>
#include <QPointer>

class KNotification;
class QAction;
class QMenu;
class QTimer;
class StateWatcher;

#ifdef KYU_HAVE_KSNI
class KStatusNotifierItem;
#else
class QSystemTrayIcon;
#endif

class TrayIcon : public QObject
{
    Q_OBJECT
public:
    TrayIcon(StateWatcher *watcher, QObject *parent = nullptr);
    ~TrayIcon() override;

Q_SIGNALS:
    void checkRequested();
    // token: the Wayland activation token of the click, if any, so the card
    // may take focus.
    void cardRequested(const QString &token);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refresh();
    void spin();
    void notifyNews(const QString &key, const QString &title, const QString &link);
    void notifyUpdates(const Kyu::TrayInput &in);

    StateWatcher *m_watcher;
    QMenu *m_menu;
    QAction *m_checkAction;
    QTimer *m_poll = nullptr;
    QTimer *m_spin = nullptr;
    int m_frame = 0;
    QString m_newsKey;
    Kyu::UpdatesMemory m_updatesMemory;
#ifdef KYU_HAVE_KNOTIFICATIONS
    QPointer<KNotification> m_news;
    QPointer<KNotification> m_updates;
#endif
#ifdef KYU_HAVE_KSNI
    KStatusNotifierItem *m_item;
#else
    QSystemTrayIcon *m_item;
#endif
};
