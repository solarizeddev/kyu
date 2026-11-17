#include "cardwindow.h"

#include "checkscheduler.h"
#include "failuretext.h"
#include "news.h"
#include "statewatcher.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
// A program next to this one (a build tree), else on PATH.
QString sibling(const QString &name)
{
    const QString path = QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
    return QFileInfo(path).isExecutable() ? path : name;
}

// kyu-update in Kyu's window, or in a terminal when the window is not built.
void startUpdate(const QStringList &args)
{
    const QString window = sibling(QStringLiteral("kyu-window"));
    if (!QStandardPaths::findExecutable(window).isEmpty() || QFileInfo(window).isExecutable()) {
        if (QProcess::startDetached(window, args)) {
            return;
        }
    }
    const QStringList command = QStringList{sibling(QStringLiteral("kyu-update"))} + args;
    if (!QProcess::startDetached(QStringLiteral("konsole"), QStringList{QStringLiteral("-e")} + command)) {
        QProcess::startDetached(QStringLiteral("xdg-terminal-exec"), command);
    }
}

QLabel *label(QWidget *parent, double scale = 1.0, bool bold = false)
{
    auto *l = new QLabel(parent);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (scale != 1.0 || bold) {
        QFont f = l->font();
        if (f.pointSizeF() > 0) {
            f.setPointSizeF(f.pointSizeF() * scale);
        } else {
            f.setPixelSize(qRound(f.pixelSize() * scale));
        }
        f.setBold(bold);
        l->setFont(f);
    }
    return l;
}

// Quieter text, in the colour scheme's own colour (follows scheme changes).
void mute(QLabel *l)
{
    l->setForegroundRole(QPalette::PlaceholderText);
}
}

CardWindow::CardWindow(StateWatcher *watcher, CheckScheduler *scheduler, QWidget *parent)
    : QWidget(parent)
    , m_watcher(watcher)
    , m_scheduler(scheduler)
{
    setWindowTitle(QStringLiteral("Kyu"));
    setFixedWidth(fontMetrics().averageCharWidth() * 46);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 16);
    layout->setSpacing(6);

    m_stamp = label(this, 0.9);
    m_stamp->setAlignment(Qt::AlignRight);
    mute(m_stamp);
    m_heading = label(this, 1.5, true);
    m_detail = label(this);
    mute(m_detail);
    m_link = label(this);
    m_link->setTextInteractionFlags(Qt::TextBrowserInteraction);
    // Opening the announcement counts as reading it: kyu-update won't ask.
    connect(m_link, &QLabel::linkActivated, this, [this](const QString &url) {
        if (QDesktopServices::openUrl(QUrl(url)) && !m_view.linkKey.isEmpty()) {
            Kyu::markNewsSeen({m_view.linkKey});
            refresh();
        }
    });

    m_mergeRow = new QWidget(this);
    auto *row = new QHBoxLayout(m_mergeRow);
    row->setContentsMargins(0, 8, 0, 0);
    m_mergeText = label(m_mergeRow);
    m_mergeButton = new QPushButton(QStringLiteral("Merge…"), m_mergeRow);
    row->addWidget(m_mergeText, 1);
    row->addWidget(m_mergeButton);
    connect(m_mergeButton, &QPushButton::clicked, this, &CardWindow::merge);
    m_mergeButton->setAutoDefault(true); // Enter on it merges, not the main button

    m_action = new QPushButton(this);
    m_action->setMinimumHeight(m_action->sizeHint().height() + 6);
    connect(m_action, &QPushButton::clicked, this, &CardWindow::act);

    layout->addWidget(m_stamp);
    layout->addWidget(m_heading);
    layout->addWidget(m_detail);
    layout->addWidget(m_link);
    layout->addWidget(m_mergeRow);
    m_gap = new QWidget(this);
    m_gap->setFixedHeight(10);
    layout->addWidget(m_gap);
    layout->addWidget(m_action);

    if (m_watcher) {
        connect(m_watcher, &StateWatcher::changed, this, &CardWindow::refresh);
    }
    // kyu-update's lock and last run, and pacman's lock: looked at every few
    // seconds while the card is open.
    m_poll = new QTimer(this);
    m_poll->setInterval(3000);
    connect(m_poll, &QTimer::timeout, this, &CardWindow::refresh);
    m_poll->start();
    refresh();
}

void CardWindow::setFixedView(const Kyu::CardView &view)
{
    m_fixed = true;
    m_poll->stop();
    present(view);
}

void CardWindow::refresh()
{
    if (m_fixed || !m_watcher) {
        return;
    }
    present(Kyu::cardView(Kyu::currentTrayInput(m_watcher->state(), m_watcher->checking(), m_watcher->loadError())));
}

void CardWindow::present(const Kyu::CardView &view)
{
    // Unchanged (most polls): leave the window as it is, selection included.
    if (m_shown && view == m_view) {
        return;
    }
    const bool newThing = !m_shown || view.kind != m_view.kind || view.action != m_view.action;
    m_shown = true;
    m_view = view;
    m_stamp->setText(view.stamp);
    m_stamp->setVisible(!view.stamp.isEmpty());
    m_heading->setText(view.heading);
    QString detail = view.detail;
    if (view.kind == Kyu::CardView::Kind::Failed) {
        const QString title = view.failureKind.isEmpty() ? QString() : failureTitle(view.failureKind);
        const QString changed = view.transactionStarted ? QStringLiteral("pacman stopped halfway.")
            : view.changed > 0 ? (view.changed == 1 ? QStringLiteral("Only 1 package was updated.") : QStringLiteral("Only %1 packages were updated.").arg(view.changed))
                               : QStringLiteral("Nothing was installed.");
        detail = title.isEmpty() ? changed : title + QStringLiteral(". ") + changed;
    }
    m_detail->setText(detail);
    m_detail->setVisible(!detail.isEmpty());
    if (view.linkText.isEmpty()) {
        m_link->hide();
    } else {
        m_link->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(view.linkUrl.toHtmlEscaped(), view.linkText.toHtmlEscaped()));
        m_link->setToolTip(view.linkUrl);
        m_link->show();
    }
    m_mergeText->setText(view.mergeText);
    m_mergeRow->setVisible(!view.mergeText.isEmpty());
    m_action->setText(view.actionText);
    const bool hasAction = view.action != Kyu::CardView::Action::None;
    m_action->setVisible(hasAction);
    m_gap->setVisible(hasAction);
    m_action->setDefault(view.primary);
    m_action->setAutoDefault(view.primary);
    // Enter presses it, not the news link; only when what to do changed, so
    // a new stamp doesn't pull the focus off the Merge… button.
    if (hasAction && newThing) {
        m_action->setFocus();
    }
    adjustSize();
}

void CardWindow::act()
{
    switch (m_view.action) {
    case Kyu::CardView::Action::Update:
        startUpdate({});
        close();
        break;
    case Kyu::CardView::Action::Merge:
        merge();
        break;
    case Kyu::CardView::Action::Check:
        if (m_scheduler) {
            m_scheduler->checkNowAsked();
        }
        break;
    case Kyu::CardView::Action::Restart: {
        // Plasma's own dialog: programs can save, and it asks.
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("/LogoutPrompt"),
                                                   QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("promptReboot"));
        const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (QMessageBox::question(this, QStringLiteral("Kyu"), QStringLiteral("Restart now? Unsaved work in open programs is lost."))
                == QMessageBox::Yes) {
                QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("reboot")});
            }
        }
        close();
        break;
    }
    case Kyu::CardView::Action::None:
        break;
    }
}

void CardWindow::merge()
{
    // pacdiff needs a terminal and sudo: in Kyu's window.
    startUpdate({QStringLiteral("--merge")});
    close();
}

void CardWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_action->isVisible()
        && !qobject_cast<QPushButton *>(focusWidget())) {
        m_action->click();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool CardWindow::event(QEvent *event)
{
    // Test kit only: says whether the card is the active window, so its
    // screenshot (with the window's decoration) is of the card.
    if (event->type() == QEvent::ActivationChange) {
        const QString path = qEnvironmentVariable("KYU_CARD_ACTIVE_FILE");
        if (!path.isEmpty()) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(isActiveWindow() ? "1" : "0");
                file.commit();
            }
        }
    }
    return QWidget::event(event);
}
