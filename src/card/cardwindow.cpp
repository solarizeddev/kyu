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
#include <QStackedWidget>
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
    // Breeze's monochrome icons are drawn for 22 and look right at the
    // style's large icon size (32 at 100 %); KDE scales it with the display.
    const int size = style()->pixelMetric(QStyle::PM_LargeIconSize, nullptr, this);
    return {size, size};
}

void IconView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    m_icon.paint(&painter, rect(), Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
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
    // A dialog: a title and a close button; no minimize or maximize.
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    // Margins and spacing are the style's (none set here).
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // Icon, then the text: heading, two lines, one more line.
    auto *header = new QHBoxLayout;
    header->setSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, this));
    m_icon = new IconView(this);
    header->addWidget(m_icon, 0, Qt::AlignTop);
    m_textColumn = new QWidget(this);
    auto *text = new QVBoxLayout(m_textColumn);
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this) / 2);
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
    header->addWidget(m_textColumn);
    header->addStretch(1); // a wider window (Details open): the space goes here
    layout->addLayout(header);

    // When, and what can be done.
    auto *footer = new QHBoxLayout;
    m_stamp = new QLabel(this);
    m_stamp->setForegroundRole(QPalette::PlaceholderText);
    m_refresh = new QToolButton(this);
    m_refresh->setAutoRaise(true);
    m_refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_refresh->setToolTip(QStringLiteral("Check now"));
    m_refresh->setAccessibleName(QStringLiteral("Check now"));
    connect(m_refresh, &QToolButton::clicked, this, [this]() {
        run(Kyu::CardView::Action::Check);
    });
    m_busy = new QProgressBar(this);
    m_busy->setRange(0, 0);
    m_busy->setTextVisible(false);
    m_details = new QPushButton(QIcon::fromTheme(QStringLiteral("view-list-details")), QStringLiteral("Details"), this);
    m_details->setCheckable(true);
    m_details->setAutoDefault(false);
    connect(m_details, &QPushButton::toggled, this, &CardWindow::showDetails);
    m_secondary = new QPushButton(this);
    m_secondary->setAutoDefault(false);
    connect(m_secondary, &QPushButton::clicked, this, [this]() {
        run(m_view.secondary);
    });
    m_action = new QPushButton(this);
    connect(m_action, &QPushButton::clicked, this, [this]() {
        run(m_view.action);
    });
    footer->addWidget(m_stamp);
    footer->addWidget(m_refresh);
    footer->addWidget(m_busy);
    footer->addStretch(1);
    footer->addWidget(m_details);
    footer->addWidget(m_secondary);
    footer->addWidget(m_action);
    layout->addLayout(footer);

    // Details: what the update changes, or the failure in the program's words.
    m_detailsArea = new QStackedWidget(this);
    m_list = new QTreeWidget(m_detailsArea);
    m_list->setHeaderLabels({QString(), QStringLiteral("Package"), QStringLiteral("Version"), QStringLiteral("Source")});
    m_list->headerItem()->setToolTip(0, QStringLiteral("Needs a restart afterwards"));
    m_list->setRootIsDecorated(false);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setUniformRowHeights(true);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_raw = new QPlainTextEdit(m_detailsArea);
    m_raw->setReadOnly(true);
    m_raw->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_raw->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_detailsArea->addWidget(m_list);
    m_detailsArea->addWidget(m_raw);
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
    // Sizes from the font: the heading 1.2 × the system font, the text
    // column about 44 characters, two lines kept for the detail and one for
    // the line below it, so the card keeps its size from state to state.
    QFont heading = font();
    if (heading.pointSizeF() > 0) {
        heading.setPointSizeF(heading.pointSizeF() * 1.2);
    } else {
        heading.setPixelSize(qRound(heading.pixelSize() * 1.2));
    }
    heading.setWeight(QFont::DemiBold);
    m_heading->setFont(heading);
    QFont small = font();
    if (small.pointSizeF() > 0) {
        small.setPointSizeF(small.pointSizeF() * 0.9);
    }
    m_stamp->setFont(small);
    const QFontMetrics metrics(font());
    m_textColumn->setFixedWidth(metrics.averageCharWidth() * 44);
    m_detail->setMinimumHeight(2 * metrics.lineSpacing());
    m_slot->setMinimumHeight(metrics.lineSpacing());
    m_busy->setFixedWidth(metrics.averageCharWidth() * 14);
    m_list->setMinimumHeight(m_list->header()->sizeHint().height() + 8 * (metrics.lineSpacing() + 4));
    m_raw->setMinimumHeight(6 * QFontMetrics(m_raw->font()).lineSpacing());
    setNote(m_noteText);
}

void CardWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        applyFonts();
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
    } else {
        setNote(view.note);
    }

    // The corner: when, or a progress bar while something runs.
    m_stamp->setText(view.stamp);
    m_stamp->setVisible(!view.busy && !view.stamp.isEmpty());
    m_refresh->setVisible(!view.busy && view.canRefresh);
    m_busy->setVisible(view.busy);

    // Details: the list of what changes, or the failure's own words.
    const bool hasDetails = !view.rows.isEmpty() || !view.raw.isEmpty();
    m_details->setVisible(hasDetails);
    if (!hasDetails && m_details->isChecked()) {
        m_details->setChecked(false);
    }
    m_list->clear();
    for (const Kyu::DetailRow &row : view.rows) {
        auto *item = new QTreeWidgetItem(m_list, {QString(), row.name, row.version, row.source});
        if (row.restart) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("system-reboot")));
            item->setToolTip(0, QStringLiteral("Needs a restart afterwards"));
            item->setToolTip(1, QStringLiteral("Needs a restart afterwards"));
        }
    }
    m_raw->setPlainText(view.raw);
    m_detailsArea->setCurrentWidget(view.rows.isEmpty() ? static_cast<QWidget *>(m_raw) : m_list);

    // The buttons: a second one when there is one, the main one last.
    m_secondary->setText(view.secondaryText);
    m_secondary->setIcon(actionIcon(view.secondary));
    m_secondary->setVisible(view.secondary != Kyu::CardView::Action::None);
    m_action->setText(view.actionText);
    m_action->setIcon(actionIcon(view.action));
    const bool hasAction = view.action != Kyu::CardView::Action::None && (view.action != Kyu::CardView::Action::ShowWindow || windowOpen);
    m_action->setVisible(hasAction);
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
    if (m_details->isChecked() != shown) {
        m_details->setChecked(shown); // comes back here through toggled
        return;
    }
    m_detailsArea->setVisible(shown);
    // Closed: the size of the contents. Open: the list can use a bigger window.
    layout()->setSizeConstraint(shown ? QLayout::SetMinimumSize : QLayout::SetFixedSize);
    if (shown) {
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
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
