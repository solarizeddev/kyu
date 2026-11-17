#pragma once

#include "traystate.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class StateWatcher;
class CheckScheduler;

// The card the tray opens: what is going on, and the one thing to do next.
// Widgets, so Plasma draws it in the user's style.
class CardWindow : public QWidget
{
    Q_OBJECT
public:
    CardWindow(StateWatcher *watcher, CheckScheduler *scheduler, QWidget *parent = nullptr);

    // For screenshots and tests: show this view instead of the system's.
    void setFixedView(const Kyu::CardView &view);

public Q_SLOTS:
    void refresh();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void present(const Kyu::CardView &view);
    void act();
    void merge();

    StateWatcher *m_watcher;
    CheckScheduler *m_scheduler;
    QTimer *m_poll;
    bool m_fixed = false;
    Kyu::CardView m_view;
    QLabel *m_stamp;
    QLabel *m_heading;
    QLabel *m_detail;
    QLabel *m_link;
    QWidget *m_mergeRow;
    QLabel *m_mergeText;
    QPushButton *m_mergeButton;
    QWidget *m_gap;
    QPushButton *m_action;
    bool m_shown = false;
};
