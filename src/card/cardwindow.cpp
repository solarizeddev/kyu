#include "cardwindow.h"

#include "checkscheduler.h"
#include "failuretext.h"
#include "news.h"
#include "statewatcher.h"

#ifdef KYU_HAVE_KWINDOWSYSTEM
#include <KWaylandExtras>
#include <KWindowSystem>
#endif

#include <QApplication>
#include <QDBusConnectionInterface>
#include <QFontDatabase>
#include <QHeaderView>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QGridLayout>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
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

#include <algorithm>
#include <memory>

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

// Kyu's update window (kyu-window) answers here while it is open.
const QString WindowService = QStringLiteral("io.github.solarizeddev.kyu.Window");

bool updateWindowOpen()
{
    const auto bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(WindowService);
}

// A theme icon, with a fallback for themes without it.
QIcon themeIcon(const QString &name)
{
    if (name == QLatin1String("system-reboot-update") && !QIcon::hasThemeIcon(name)) {
        return QIcon::fromTheme(QStringLiteral("system-reboot"));
    }
    return QIcon::fromTheme(name);
}

QIcon actionIcon(Kyu::CardView::Action action)
{
    switch (action) {
    case Kyu::CardView::Action::Update:
        return QIcon::fromTheme(QStringLiteral("system-upgrade"));
    case Kyu::CardView::Action::Restart:
        return QIcon::fromTheme(QStringLiteral("system-reboot"));
    case Kyu::CardView::Action::Merge:
        return QIcon::fromTheme(QStringLiteral("merge"));
    case Kyu::CardView::Action::Check:
        return QIcon::fromTheme(QStringLiteral("view-refresh"));
    case Kyu::CardView::Action::ShowWindow:
        return QIcon::fromTheme(QStringLiteral("window"));
    case Kyu::CardView::Action::None:
        break;
    }
    return {};
}

QLabel *textLabel(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}
}

IconView::IconView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void IconView::setIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

QSize IconView::sizeHint() const
{
    return size();
}

void IconView::paintEvent(QPaintEvent *)
{
    // Breeze's monochrome icons are drawn for 22 px: asked for at that size
    // and rendered for this widget's (32 at 100 %), so the vector is drawn
    // sharp at the size shown, not a small icon in a big box.
    constexpr int design = 22;
    QPainter painter(this);
    const QPixmap pixmap = m_icon.pixmap(QSize(design, design), devicePixelRatioF() * width() / qreal(design), isEnabled() ? QIcon::Normal : QIcon::Disabled);
    painter.drawPixmap(rect(), pixmap);
}

void IconView::changeEvent(QEvent *event)
{
    // A new colour scheme: the icon is drawn again in its colours.
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

CardWindow::CardWindow(StateWatcher *watcher, CheckScheduler *scheduler, QWidget *parent)
    : QWidget(parent)
    , m_watcher(watcher)
    , m_scheduler(scheduler)
{
    setWindowTitle(QStringLiteral("Kyu"));
    // A dialog with a title and a close button. Its fixed size takes
    // maximize away; Wayland has no way to ask for no minimize (minimizing
    // closes it, see changeEvent).
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    // Margins and spacing are the style's (none set here).
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setSpacing(0); // the gaps below are set one by one
    const int across = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, this);
    const int down = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);

    // The icon beside the heading (as in Plasma's notifications), then the
    // text: heading, two lines, and a third (a link to read first, or a
    // note) when there is one.
    m_grid = new QGridLayout;
    m_grid->setHorizontalSpacing(2 * across);
    m_grid->setVerticalSpacing(0);
    m_icon = new IconView(this);
    m_textColumn = new QWidget(this);
    auto *text = new QVBoxLayout(m_textColumn);
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(down / 2);
    m_heading = textLabel(m_textColumn);
    m_detail = textLabel(m_textColumn);
    m_detail->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_slot = new QLabel(m_textColumn);
    m_slot->setTextInteractionFlags(Qt::TextBrowserInteraction);
    // Opening the announcement counts as reading it: kyu-update won't ask.
    connect(m_slot, &QLabel::linkActivated, this, [this](const QString &url) {
        if (QDesktopServices::openUrl(QUrl(url)) && !m_view.linkKey.isEmpty()) {
            Kyu::markNewsSeen({m_view.linkKey});
            refresh();
        }
    });
    text->addWidget(m_heading);
    text->addWidget(m_detail);
    text->addWidget(m_slot);
    // While something runs: a progress bar under the text, as wide as it.
    m_busy = new QProgressBar(m_textColumn);
    m_busy->setRange(0, 0);
    m_busy->setTextVisible(false);
    m_busy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    text->addSpacing(down / 2);
    text->addWidget(m_busy);
    // "▸ Details" under the text, as in KDE's message boxes: a disclosure,
    // not a button beside the actions; what it shows opens below the buttons.
    m_details = new QToolButton(m_textColumn);
    m_details->setText(QStringLiteral("Details"));
    m_details->setAutoRaise(true);
    m_details->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_details->setArrowType(Qt::RightArrow);
    connect(m_details, &QToolButton::clicked, this, [this]() {
        showDetails(!m_detailsShown);
    });
    text->addWidget(m_details, 0, Qt::AlignLeft);
    m_grid->addWidget(m_icon, 0, 0, Qt::AlignTop);
    m_grid->addWidget(m_textColumn, 0, 1);
    m_grid->setColumnStretch(1, 1); // a wider window (Details open): the text gets the room
    layout->addLayout(m_grid);
    layout->addSpacing(2 * down);


    // The buttons, on the right: what to do, the main one last.
    m_footer = new QHBoxLayout;
    m_footer->setSpacing(across); // the root's 0 is not for between buttons
    m_footerHeight = new QWidget(this);
    m_secondary = new QPushButton(this);
    m_secondary->setAutoDefault(false);
    connect(m_secondary, &QPushButton::clicked, this, [this]() {
        run(m_view.secondary);
    });
    m_action = new QPushButton(this);
    connect(m_action, &QPushButton::clicked, this, [this]() {
        run(m_view.action);
    });
    m_footer->addStretch(1);
    m_footer->addWidget(m_footerHeight); // no width: keeps a button's height without buttons
    m_footer->addWidget(m_secondary);
    m_footer->addWidget(m_action);
    layout->addLayout(m_footer);

    // Details: what the update changes, or the failure in the program's words.
    m_detailsArea = new QWidget(this);
    auto *details = new QVBoxLayout(m_detailsArea);
    details->setContentsMargins(0, down, 0, 0);
    details->setSpacing(0);
    m_list = new QTreeWidget(m_detailsArea);
    m_list->setHeaderLabels({QString(), QStringLiteral("Package"), QStringLiteral("Version"), QStringLiteral("Source")});
    m_list->headerItem()->setToolTip(0, QStringLiteral("Needs a restart afterwards"));
    m_list->setRootIsDecorated(false);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setUniformRowHeights(true);
    m_list->setTextElideMode(Qt::ElideMiddle);
    m_list->header()->setStretchLastSection(true);
    m_list->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_list->header()->setMinimumSectionSize(1);
    m_raw = new QPlainTextEdit(m_detailsArea);
    m_raw->setReadOnly(true);
    m_raw->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_raw->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    details->addWidget(m_list);
    details->addWidget(m_raw);
    m_detailsArea->hide();
    layout->addWidget(m_detailsArea, 1);

    applyFonts();

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

void CardWindow::applyFonts()
{
    // Sizes from the font: the heading 1.2 × the system font, semibold; the
    // text column about 42 characters; two lines kept for the detail.
    QFont heading = font();
    if (heading.pointSizeF() > 0) {
        heading.setPointSizeF(heading.pointSizeF() * 1.2);
    } else {
        heading.setPixelSize(qRound(heading.pixelSize() * 1.2));
    }
    // KDE's font carries a style name ("Regular"), and with one set Qt
    // ignores the weight: cleared, so the heading is really semibold.
    heading.setStyleName(QString());
    heading.setWeight(QFont::DemiBold);
    m_heading->setFont(heading);
    const QFontMetrics metrics(font());
    m_textColumn->setFixedWidth(metrics.averageCharWidth() * 42);
    m_detail->setMinimumHeight((m_view.busy ? 1 : 2) * metrics.lineSpacing());
    // The icon at the style's large size (32 at 100 %), top-left, level
    // with the heading.
    const int large = style()->pixelMetric(QStyle::PM_LargeIconSize, nullptr, this);
    m_icon->setFixedSize(large, large);
    const int small = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    m_list->header()->resizeSection(0, small + 8);
    keepFooterHeight();
    sizeDetails();
    setNote(m_noteText);
}

void CardWindow::keepFooterHeight()
{
    // As tall as a button with its icon and text (the Details button always
    // has both), whether any button shows or not.
    m_details->ensurePolished();
    m_footerHeight->setFixedSize(0, m_details->sizeHint().height());
}

void CardWindow::sizeDetails()
{
    // The list as wide as its columns (no name cut), at most eight rows tall;
    // the failure's lines as tall as they are (3 to 12).
    const int across = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, this);
    for (int column = 1; column < m_list->columnCount(); ++column) {
        m_list->resizeColumnToContents(column);
        m_list->setColumnWidth(column, m_list->columnWidth(column) + across); // room between columns
    }
    m_list->setColumnWidth(0, style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this) + 8);
    int width = 2 * m_list->frameWidth() + style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
    for (int column = 0; column < m_list->columnCount(); ++column) {
        width += m_list->isColumnHidden(column) ? 0 : m_list->columnWidth(column);
    }
    m_list->setMinimumWidth(width);
    const int rows = m_list->topLevelItemCount();
    const int row = rows > 0 ? m_list->sizeHintForRow(0) : fontMetrics().height() + 6;
    // As tall as its rows (up to 10; more scroll).
    const int listHeight = m_list->header()->sizeHint().height() + qBound(1, rows, 10) * row + 2 * m_list->frameWidth();
    if (rows <= 10) {
        m_list->setFixedHeight(listHeight);
    } else {
        m_list->setMinimumHeight(listHeight);
        m_list->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    // Lines as they wrap in the card's width.
    const QFontMetrics mono(m_raw->font());
    const int perLine = qMax(20, (m_textColumn->width() + m_icon->sizeHint().width() + m_grid->horizontalSpacing()) / qMax(1, mono.averageCharWidth()) - 2);
    int lines = 0;
    for (const QString &line : m_raw->toPlainText().split(QLatin1Char('\n'))) {
        lines += qMax(1, int((line.size() + perLine - 1) / perLine));
    }
    m_raw->setFixedHeight(qBound(3, lines + 1, 12) * mono.lineSpacing() + 2 * m_raw->frameWidth()
                          + 2 * int(m_raw->document()->documentMargin()));
}

void CardWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        applyFonts();
    }
    // Minimizing the card means "not now": it closes (the tray opens it again).
    if (event->type() == QEvent::WindowStateChange && isMinimized()) {
        QTimer::singleShot(0, this, &QWidget::close);
    }
    QWidget::changeEvent(event);
}

void CardWindow::setNote(const QString &text)
{
    // One line: a long path is cut in the middle, all of it in the tooltip.
    m_noteText = text;
    if (!m_view.linkText.isEmpty()) {
        return;
    }
    m_slot->setTextFormat(Qt::PlainText);
    m_slot->setText(m_slot->fontMetrics().elidedText(text, Qt::ElideMiddle, m_textColumn->maximumWidth()));
    m_slot->setToolTip(text == m_slot->text() ? QString() : text);
    m_slot->setVisible(!text.isEmpty());
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
    // "Show window" only while Kyu's window is there to show (an update in a
    // plain terminal has none).
    const bool windowOpen = view.action == Kyu::CardView::Action::ShowWindow && (m_fixed || updateWindowOpen());
    // Unchanged (most polls): leave the window as it is, selection included.
    if (m_shown && view == m_view && windowOpen == m_windowOpen) {
        return;
    }
    m_windowOpen = windowOpen;
    const bool newThing = !m_shown || view.kind != m_view.kind || view.action != m_view.action;
    m_shown = true;
    m_view = view;

    m_icon->setIcon(themeIcon(view.icon));
    m_heading->setText(view.heading);
    QString detail = view.detail;
    // A failed update: what went wrong (the failure's title), then what changed.
    if (view.kind == Kyu::CardView::Kind::Failed && !view.failureKind.isEmpty()) {
        if (const QString title = failureTitle(view.failureKind); !title.isEmpty()) {
            detail = title + QStringLiteral(".\n") + detail;
        }
    }
    m_detail->setText(detail);
    if (!view.linkText.isEmpty()) {
        m_slot->setTextFormat(Qt::RichText);
        const QString shown = m_slot->fontMetrics().elidedText(view.linkText, Qt::ElideRight, m_textColumn->maximumWidth());
        m_slot->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(view.linkUrl.toHtmlEscaped(), shown.toHtmlEscaped()));
        m_slot->setToolTip(view.linkText + QLatin1Char('\n') + view.linkUrl);
        m_slot->show();
    } else {
        setNote(view.note);
    }

    // While something runs: a progress bar under the text, and one line of
    // text kept instead of two (the card stays about as tall).
    m_busy->setVisible(view.busy);
    m_detail->setMinimumHeight((view.busy ? 1 : 2) * QFontMetrics(font()).lineSpacing());

    // Details: the list of what changes, or the failure's own words.
    const bool hasDetails = !view.rows.isEmpty() || !view.raw.isEmpty();
    m_details->setVisible(hasDetails);
    if (!hasDetails && m_detailsShown) {
        showDetails(false);
    }
    m_list->clear();
    for (const Kyu::DetailRow &row : view.rows) {
        auto *item = new QTreeWidgetItem(m_list, {QString(), row.name, row.version, row.source});
        item->setToolTip(1, row.name);
        item->setToolTip(2, row.version);
        if (row.restart) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("system-reboot")));
            item->setToolTip(0, QStringLiteral("Needs a restart afterwards"));
            item->setToolTip(1, QStringLiteral("Needs a restart afterwards"));
        }
    }
    // The restart column only when a row needs a restart.
    m_list->setColumnHidden(0, std::none_of(view.rows.cbegin(), view.rows.cend(), [](const Kyu::DetailRow &row) {
        return row.restart;
    }));
    keepFooterHeight();
    m_raw->setPlainText(view.raw);
    m_list->setVisible(!view.rows.isEmpty());
    m_raw->setVisible(view.rows.isEmpty());
    sizeDetails();

    // The buttons: a second one when there is one, the main one last.
    m_secondary->setText(view.secondaryText);
    m_secondary->setIcon(actionIcon(view.secondary));
    m_secondary->setVisible(view.secondary != Kyu::CardView::Action::None);
    m_action->setText(view.actionText);
    m_action->setIcon(actionIcon(view.action));
    const bool hasAction = view.action != Kyu::CardView::Action::None && (view.action != Kyu::CardView::Action::ShowWindow || windowOpen);
    m_action->setVisible(hasAction);
    m_action->setEnabled(view.actionEnabled);
    // No buttons at all (the first check): no empty button row.
    m_footerHeight->setVisible(hasAction || hasDetails || view.secondary != Kyu::CardView::Action::None);
    m_action->setDefault(view.primary);
    m_action->setAutoDefault(view.primary);
    // Enter presses it, not the news link; only when what to do changed, so
    // a new stamp doesn't pull the focus off another button.
    if (hasAction && newThing) {
        m_action->setFocus();
    }
}

void CardWindow::showDetails(bool shown)
{
    m_detailsShown = shown;
    m_detailsArea->setVisible(shown);
    m_details->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
    // Open: the window can grow, and the text with it; closed: its own width.
    const int column = QFontMetrics(font()).averageCharWidth() * 42;
    if (shown) {
        m_textColumn->setMinimumWidth(column);
        m_textColumn->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        m_textColumn->setFixedWidth(column);
    }
    // Closed: the size of the contents. Open: the list can use a bigger window.
    layout()->setSizeConstraint(shown ? QLayout::SetMinimumSize : QLayout::SetFixedSize);
    if (shown) {
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        sizeDetails(); // measured now that it shows (rows have their real height)
    }
    adjustSize();
}

void CardWindow::run(Kyu::CardView::Action action)
{
    switch (action) {
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
    case Kyu::CardView::Action::ShowWindow:
        showUpdateWindow();
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

void CardWindow::showUpdateWindow()
{
    // Kyu's window comes forward; on Wayland it needs a token from this
    // window's click to be allowed to take the focus.
    const auto send = [](const QString &token) {
        auto call = QDBusMessage::createMethodCall(WindowService, QStringLiteral("/"), WindowService, QStringLiteral("Activate"));
        call.setArguments({token});
        QDBusConnection::sessionBus().asyncCall(call);
    };
#ifdef KYU_HAVE_KWINDOWSYSTEM
    if (KWindowSystem::isPlatformWayland() && windowHandle()) {
        const quint32 serial = KWaylandExtras::lastInputSerial(windowHandle());
        // Once: with the token, or without it after a second (no answer).
        auto done = std::make_shared<bool>(false);
        auto finish = [this, send, done](const QString &token) {
            if (*done) {
                return;
            }
            *done = true;
            send(token);
            close();
        };
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(KWaylandExtras::self(), &KWaylandExtras::xdgActivationTokenArrived, this,
                              [serial, connection, finish](int arrived, const QString &token) {
                                  if (quint32(arrived) != serial) {
                                      return;
                                  }
                                  QObject::disconnect(*connection);
                                  finish(token);
                              });
        QTimer::singleShot(1000, this, [connection, finish]() {
            QObject::disconnect(*connection);
            finish(QString());
        });
        KWaylandExtras::requestXdgActivationToken(windowHandle(), serial, QStringLiteral(KYU_APP_ID));
        return;
    }
#endif
    send(QString());
    close();
}

void CardWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    // Enter: the focused button (a card is not a QDialog, which would do
    // this), else the main one.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (auto *button = qobject_cast<QAbstractButton *>(focusWidget()); button && button->isVisible()) {
            button->click();
            return;
        }
        if (m_action->isVisible()) {
            m_action->click();
            return;
        }
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
