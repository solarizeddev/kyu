#pragma once

#include "traystate.h"

#include <QIcon>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class StateWatcher;
class CheckScheduler;

// A theme icon painted at the window's own scale (a pixmap in a label would
// be blurry at fractional scales), repainted when the palette changes, so
// Breeze's monochrome icons follow the colour scheme.
class IconView : public QWidget
{
public:
    explicit IconView(QWidget *parent = nullptr);
    void setIcon(const QIcon &icon);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QIcon m_icon;
};

// The card the tray opens: a small dialog with an icon, a heading, two
// lines, one more line (a link to read first or a note), and at most two
// buttons; Details below on request. Laid out from the style's spacing and
// the font, so it follows font size and display scale; its size comes from
// its contents (resizable only while Details are open).
class CardWindow : public QWidget
{
    Q_OBJECT
public:
    CardWindow(StateWatcher *watcher, CheckScheduler *scheduler, QWidget *parent = nullptr);

    // For screenshots and tests: show this view instead of the system's.
    void setFixedView(const Kyu::CardView &view);
    // For screenshots: Details open.
    void showDetails(bool shown);

public Q_SLOTS:
    void refresh();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void present(const Kyu::CardView &view);
    void run(Kyu::CardView::Action action);
    void merge();
    void showUpdateWindow();
    void applyFonts();
    void setNote(const QString &text);

    StateWatcher *m_watcher;
    CheckScheduler *m_scheduler;
    QTimer *m_poll;
    bool m_fixed = false;
    Kyu::CardView m_view;
    QString m_noteText;
    IconView *m_icon;
    QLabel *m_heading;
    QLabel *m_detail;
    QLabel *m_slot;
    QWidget *m_textColumn;
    QLabel *m_stamp;
    QToolButton *m_refresh;
    QProgressBar *m_busy;
    QPushButton *m_details;
    QPushButton *m_secondary;
    QPushButton *m_action;
    QStackedWidget *m_detailsArea;
    QTreeWidget *m_list;
    QPlainTextEdit *m_raw;
    bool m_shown = false;
    bool m_windowOpen = false; // Kyu's update window was there when last shown
};
