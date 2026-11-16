#pragma once

#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
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
    void panelRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refresh();

    StateWatcher *m_watcher;
    QMenu *m_menu;
    QAction *m_checkAction;
#ifdef KYU_HAVE_KSNI
    KStatusNotifierItem *m_item;
#else
    QSystemTrayIcon *m_item;
#endif
};
