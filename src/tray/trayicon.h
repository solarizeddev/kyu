#pragma once

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
    void notifyNews(const QString &key, const QString &title, const QString &link, const QString &source);

    StateWatcher *m_watcher;
    QMenu *m_menu;
    QAction *m_checkAction;
    QTimer *m_poll = nullptr;
    QTimer *m_spin = nullptr;
    int m_frame = 0;
    QString m_newsKey;
#ifdef KYU_HAVE_KNOTIFICATIONS
    QPointer<KNotification> m_news;
#endif
#ifdef KYU_HAVE_KSNI
    KStatusNotifierItem *m_item;
#else
    QSystemTrayIcon *m_item;
#endif
};
