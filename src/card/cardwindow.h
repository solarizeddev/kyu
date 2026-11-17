#pragma once

#include "traystate.h"

#include <QAbstractButton>
#include <QIcon>
#include <QWidget>

class QLabel;
class QProgressBar;
class QSpacerItem;
class QToolButton;
class QPlainTextEdit;
class QPushButton;
class QGridLayout;
class QHBoxLayout;
class QVBoxLayout;
class QTimer;
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

// "▸ Details" as KDE's message boxes have it: the arrow flush with the
// text's left edge, the label after it, no button frame.
class Disclosure : public QAbstractButton
{
public:
    explicit Disclosure(const QString &text, QWidget *parent = nullptr);
    void setOpen(bool open);
    // The chevron centred in a column this wide (the icon's), the label
    // after it at the text's edge (gap further on).
    void setGutter(int width, int gap);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int chevron() const; // the chevron's size: the font's x-height
    int labelX() const;
    bool m_open = false;
    int m_gutter = 0;
    int m_gap = 0;
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
    // The band as tall as its lines as laid out (once the window has its
    // size): measured, not estimated.
    void fitBand();

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
    void sizeDetails();
    void keepFooterHeight();

    StateWatcher *m_watcher;
    CheckScheduler *m_scheduler;
    QTimer *m_poll;
    bool m_fixed = false;
    Kyu::CardView m_view;
    IconView *m_icon;
    QLabel *m_heading;
    QLabel *m_detail;
    QLabel *m_muted;
    QLabel *m_slot;
    QWidget *m_textColumn;
    QProgressBar *m_busy;
    QWidget *m_busyRow; // the bar, a little below its sentence
    Disclosure *m_details;
    QPushButton *m_measure; // never shown: how tall a button with icon and text is
    QPushButton *m_secondary;
    QPushButton *m_action;
    QWidget *m_detailsArea; // the band: edge to edge, under a hairline
    QVBoxLayout *m_top = nullptr; // the text and the button row, in the style's margins
    QGridLayout *m_grid;
    QHBoxLayout *m_footer;
    QWidget *m_footerHeight; // the button row as tall without buttons (no width)
    QWidget *m_footerRow = nullptr; // Details and the buttons, twice a line's spacing below the text
    QPlainTextEdit *m_raw;
    bool m_shown = false;
    bool m_detailsShown = false;
    bool m_windowOpen = false; // Kyu's update window was there when last shown
};
