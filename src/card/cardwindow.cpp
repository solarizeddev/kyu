#include "cardwindow.h"

#include "checkscheduler.h"
#include "failuretext.h"
#include "news.h"
#include "statewatcher.h"

#ifdef KYU_HAVE_KWINDOWSYSTEM
#include <KWaylandExtras>
#include <kwindowsystem_version.h>
#include <KWindowSystem>
#endif

#include <QApplication>
#include <QDBusConnectionInterface>
#include <QFontDatabase>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QFuture>
#include <QGridLayout>
#include <QStyle>
#include <QStyleOption>
#include <QToolButton>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>
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
#include <QTextLayout>
#include <QTimer>
#include <QtMath>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace
{
// Breeze's monochrome status icons, as the tray shows them.
constexpr int IconSize = 22;
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

Disclosure::Disclosure(const QString &text, QWidget *parent)
    : QAbstractButton(parent)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void Disclosure::setOpen(bool open)
{
    m_open = open;
    update();
}

void Disclosure::setGutter(int width, int gap)
{
    m_gutter = width;
    m_gap = gap;
    updateGeometry();
    update();
}

int Disclosure::labelX() const
{
    if (m_gutter > 0) {
        return m_gutter + m_gap;
    }
    return chevron() + style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, this) / 2;
}

QSize Disclosure::sizeHint() const
{
    const QFontMetrics metrics(font());
    return {labelX() + metrics.horizontalAdvance(text()) + 2, metrics.height() + 2};
}

int Disclosure::chevron() const
{
    return qMax(6, QFontMetrics(font()).xHeight());
}

void Disclosure::paintEvent(QPaintEvent *)
{
    // A chevron as Breeze draws them (› closed, ⌄ open), its ink from the
    // left edge, where the text above starts; then the label.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor colour = palette().color(QPalette::WindowText);
    QPen pen(colour, 1.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    const qreal size = chevron();
    const qreal cy = height() / 2.0;
    const qreal half = size / 2.0;
    // In the icon's column: centred under the icon.
    const qreal left = m_gutter > 0 ? (m_gutter - size) / 2.0 : 0.0;
    if (m_open) {
        painter.drawPolyline(QPolygonF{QPointF(left + 0.6, cy - half / 2), QPointF(left + half, cy + half / 2), QPointF(left + size - 0.6, cy - half / 2)});
    } else {
        painter.drawPolyline(QPolygonF{QPointF(left + half / 2, cy - half), QPointF(left + half / 2 + half, cy), QPointF(left + half / 2, cy + half)});
    }
    QFont label = font();
    label.setUnderline(underMouse());
    painter.setFont(label);
    painter.setPen(colour);
    const int x = labelX();
    painter.drawText(QRect(x, 0, width() - x, height()), Qt::AlignLeft | Qt::AlignVCenter, text());
    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
    }
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
    // The quieter line: same size, the colour scheme's placeholder colour.
    m_muted = textLabel(m_textColumn);
    m_muted->setForegroundRole(QPalette::PlaceholderText);
    m_slot = new QLabel(m_textColumn);
    m_slot->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_slot->setContentsMargins(0, down / 2, 0, 0); // a little apart from the text above
    // Opening the announcement counts as reading it: kyu-update won't ask.
    connect(m_slot, &QLabel::linkActivated, this, [this](const QString &url) {
        if (QDesktopServices::openUrl(QUrl(url)) && !m_view.linkKey.isEmpty()) {
            Kyu::markNewsSeen({m_view.linkKey});
            refresh();
        }
    });
    text->addWidget(m_heading);
    text->addWidget(m_detail);
    text->addWidget(m_muted);
    text->addWidget(m_slot);
    // While something runs: a progress bar right under its sentence, as wide
    // as the text.
    m_busyRow = new QWidget(m_textColumn);
    auto *busyRow = new QVBoxLayout(m_busyRow);
    busyRow->setContentsMargins(0, down / 2, 0, 0);
    m_busy = new QProgressBar(m_busyRow);
    m_busy->setRange(0, 0);
    m_busy->setTextVisible(false);
    m_busy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    busyRow->addWidget(m_busy);
    text->addWidget(m_busyRow);
    m_grid->addWidget(m_icon, 0, 0, Qt::AlignTop);
    m_grid->addWidget(m_textColumn, 0, 1);
    m_grid->setColumnStretch(1, 1); // a wider window (Details open): the text gets the room

    // Two parts. Above, in the style's margins: the text, then one row
    // with "› Details" (its chevron in the icon's column, its word at the
    // text's edge) on the left and the buttons on the right. Below, when
    // Details is open: a band from window edge to window edge, under a
    // hairline and down to the window's bottom (as Dolphin's terminal
    // panel), its text at the text's edge again. One left edge for all text.
    const int left = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this);
    const int top = style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, this);
    const int right = style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this);
    const int bottom = style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_top = new QVBoxLayout;
    m_top->setContentsMargins(left, top, right, bottom);
    m_top->setSpacing(0);
    m_top->addLayout(m_grid);
    m_top->addSpacing(2 * down); // twice a line's spacing above the buttons
    layout->addLayout(m_top);

    m_details = new Disclosure(QStringLiteral("Details"), this);
    m_details->setGutter(IconSize, 2 * across);
    connect(m_details, &QAbstractButton::clicked, this, [this]() {
        showDetails(!m_detailsShown);
    });

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
    m_measure = new QPushButton(QIcon::fromTheme(QStringLiteral("system-upgrade")), QStringLiteral("Update"), this);
    m_measure->hide();
    connect(m_action, &QPushButton::clicked, this, [this]() {
        run(m_view.action);
    });
    m_footer->addWidget(m_details);
    m_footer->addSpacing(2 * across);
    m_footer->addStretch(1);
    m_footer->addWidget(m_footerHeight); // no width: keeps a button's height without buttons
    m_footer->addWidget(m_secondary);
    m_footer->addWidget(m_action);
    m_top->addLayout(m_footer);
    setTabOrder(m_details, m_secondary);
    setTabOrder(m_secondary, m_action);

    // The band: what the update changes, as pacman lists it, or the failure
    // in the program's words, in the fixed-width font on the palette's Base
    // (a view's colour; not a terminal's: nothing to type), with no frame
    // of its own.
    m_detailsArea = new QWidget(this);
    m_detailsArea->setAutoFillBackground(true);
    m_detailsArea->setBackgroundRole(QPalette::Base);
    auto *band = new QVBoxLayout(m_detailsArea);
    band->setContentsMargins(0, 0, 0, 0);
    band->setSpacing(0);
    auto *hairline = new QFrame(m_detailsArea);
    hairline->setFrameShape(QFrame::HLine);
    hairline->setFrameShadow(QFrame::Sunken); // as KSeparator: Breeze draws one line
    band->addWidget(hairline);
    auto *pad = new QVBoxLayout;
    pad->setContentsMargins(left + IconSize + 2 * across, down, right, down);
    m_raw = new QPlainTextEdit(m_detailsArea);
    m_raw->setReadOnly(true);
    m_raw->setFrameShape(QFrame::NoFrame);
    m_raw->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_raw->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_raw->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_raw->document()->setDocumentMargin(0); // the band's margins place the text
    pad->addWidget(m_raw);
    band->addLayout(pad, 1);
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
    // ignores the weight: cleared. Bold (a semibold face is not always there,
    // and the heading came out regular).
    heading.setStyleName(QString());
    heading.setWeight(QFont::Bold);
    m_heading->setFont(heading);
    const QFontMetrics metrics(font());
    m_textColumn->setFixedWidth(metrics.averageCharWidth() * 42);
    // The icon at the size Breeze draws its monochrome status icons (and the
    // tray shows them): 22 px, sharp; top-left, level with the heading.
    m_icon->setFixedSize(IconSize, IconSize);
    keepFooterHeight();
    sizeDetails();
}

void CardWindow::keepFooterHeight()
{
    // As tall as a button with its icon and text, whether any shows or not.
    m_measure->ensurePolished();
    m_footerHeight->setFixedSize(0, m_measure->sizeHint().height());
}

void CardWindow::sizeDetails()
{
    constexpr int MaxLines = 16;
    // The box as tall as its lines (3 to 16; more scroll: three blocks of
    // a usual update fit). The package list
    // is not wrapped: the box is as wide as its longest line (up to 80
    // characters; more scroll sideways). The failure's lines wrap in the
    // card's width.
    const QFontMetrics mono(m_raw->font());
    const int margin = int(m_raw->document()->documentMargin());
    const int frame = 2 * m_raw->frameWidth() + 2 * margin;
    const QStringList text = m_raw->toPlainText().split(QLatin1Char('\n'));
    int lines = 0;
    bool sideways = false;
    if (!m_view.rows.isEmpty()) {
        m_raw->setLineWrapMode(QPlainTextEdit::NoWrap);
        // Measured as drawn (a character is not a whole number of pixels;
        // the bold header lines in bold), a pixel for the cursor.
        QFont boldFont = m_raw->font();
        boldFont.setBold(true);
        const QFontMetricsF regular(m_raw->font()), bold(boldFont);
        const qreal most = regular.horizontalAdvance(QString(80, QLatin1Char('M')));
        qreal widest = 0;
        for (const QString &line : text) {
            widest = qMax(widest, qMax(regular.horizontalAdvance(line), bold.horizontalAdvance(line)));
        }
        sideways = widest > most;
        // Room for the scroll bar a longer list gets, so it covers no column.
        const int bar = text.size() > MaxLines ? style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_raw) : 0;
        m_raw->setMinimumWidth(qCeil(qMin(widest, most)) + frame + 2 + bar);
        lines = int(text.size());
    } else {
        m_raw->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        m_raw->setMinimumWidth(0);
        // Lines as the box wraps them (at word boundaries, in the card's
        // width less the frame and margins).
        const qreal wrapWidth = m_textColumn->width() - frame;
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        for (const QString &line : text) {
            QTextLayout layout(line, m_raw->font());
            layout.setTextOption(option);
            layout.beginLayout();
            for (QTextLine l = layout.createLine(); l.isValid(); l = layout.createLine()) {
                l.setLineWidth(wrapWidth);
                ++lines;
            }
            layout.endLayout();
        }
    }
    // Scroll bars only when there is more than fits (a scroll bar would
    // narrow the box and change the wrapping just measured).
    m_raw->setVerticalScrollBarPolicy(lines > MaxLines ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_raw->setHorizontalScrollBarPolicy(sideways ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    const int bar = sideways ? style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_raw) : 0;
    // A line's real height is not a whole number of pixels (Hack's is
    // about 16.4): the fractional one, the larger of regular and bold, so
    // twelve lines do not come up short.
    QFont boldLine = m_raw->font();
    boldLine.setBold(true);
    const qreal line = qMax(QFontMetricsF(m_raw->font()).lineSpacing(), QFontMetricsF(boldLine).lineSpacing());
    int height = qCeil(qBound(3, lines, MaxLines) * line) + mono.descent();
    if (!m_view.rows.isEmpty() && lines <= MaxLines) {
        // Not wrapped: the lines' heights as the box lays them out.
        qreal laid = 0;
        for (QTextBlock block = m_raw->document()->begin(); block.isValid(); block = block.next()) {
            laid += m_raw->document()->documentLayout()->blockBoundingRect(block).height();
        }
        height = qMax(height, qCeil(laid) + 1);
    }
    m_raw->setFixedHeight(height + frame + bar);
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
    QString muted = view.muted;
    // A failed update: what went wrong (the failure's title), then, muted,
    // what changed. One short line each, so neither wraps.
    if (view.kind == Kyu::CardView::Kind::Failed && !view.failureKind.isEmpty()) {
        if (const QString title = failureTitle(view.failureKind); !title.isEmpty()) {
            muted = muted.isEmpty() ? detail : detail + QLatin1Char('\n') + muted;
            detail = title + QLatin1Char('.');
        }
    }
    // A path is not wrapped (it would break at a "/"): one line, shortened
    // in the middle when it is too long, the whole of it in the tooltip.
    m_detail->setWordWrap(!view.detailPath);
    m_detail->setToolTip(QString());
    if (view.detailPath) {
        const QString shown = m_detail->fontMetrics().elidedText(detail, Qt::ElideMiddle, m_textColumn->maximumWidth());
        if (shown != detail) {
            m_detail->setToolTip(detail);
        }
        detail = shown;
    }
    m_detail->setText(detail);
    m_detail->setVisible(!detail.isEmpty());
    m_muted->setText(muted);
    m_muted->setVisible(!muted.isEmpty());
    if (!view.linkText.isEmpty()) {
        m_slot->setTextFormat(Qt::RichText);
        const QString shown = m_slot->fontMetrics().elidedText(view.linkText, Qt::ElideRight, m_textColumn->maximumWidth());
        m_slot->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(view.linkUrl.toHtmlEscaped(), shown.toHtmlEscaped()));
        m_slot->setToolTip(view.linkText + QLatin1Char('\n') + view.linkUrl);
        m_slot->show();
    } else {
        m_slot->clear();
        m_slot->hide();
    }

    // While something runs: a progress bar under the text, and one line of
    // text kept instead of two (the card stays about as tall).
    m_busyRow->setVisible(view.busy);

    // Details: the list of what changes, or the failure's own words.
    const bool hasDetails = !view.rows.isEmpty() || !view.raw.isEmpty();
    m_details->setVisible(hasDetails);
    if (!hasDetails && m_detailsShown) {
        showDetails(false);
    }
    keepFooterHeight();
    if (view.rows.isEmpty()) {
        m_raw->setPlainText(view.raw);
    } else {
        // pacman's list, its header lines bold as pacman prints them.
        m_raw->clear();
        QTextCursor cursor(m_raw->document());
        QTextCharFormat plain;
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        bool first = true;
        for (const Kyu::DetailLine &line : Kyu::detailLines(view.rows)) {
            if (!first) {
                cursor.insertBlock();
            }
            first = false;
            cursor.insertText(line.text, line.header ? bold : plain);
        }
        m_raw->moveCursor(QTextCursor::Start);
    }
    sizeDetails();

    // The buttons: a second one when there is one, the main one last.
    // Each button with its first letter as its Alt key (Breeze underlines
    // it while Alt is held); no two buttons on one card share a letter.
    const auto withKey = [](QString text) {
        return QLatin1Char('&') + text.replace(QLatin1Char('&'), QStringLiteral("&&"));
    };
    m_secondary->setText(withKey(view.secondaryText));
    m_secondary->setIcon(actionIcon(view.secondary));
    m_secondary->setVisible(view.secondary != Kyu::CardView::Action::None);
    m_secondary->setEnabled(view.secondaryEnabled);
    m_action->setText(withKey(view.actionText));
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
    // Open: the band follows the button row a line's spacing below, and
    // runs to the window's bottom.
    {
        const QMargins m = m_top->contentsMargins();
        const int down = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);
        m_top->setContentsMargins(m.left(), m.top(), m.right(), shown ? down : style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this));
    }
    m_details->setOpen(shown);
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
        QTimer::singleShot(1000, this, [finish]() {
            finish(QString());
        });
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(6, 19, 0)
        // KF 6.19 and later: the token as a future (for this window's last click).
        KWaylandExtras::xdgActivationToken(windowHandle(), QStringLiteral(KYU_APP_ID)).then(this, [finish](const QString &token) {
            finish(token);
        });
#else
        const quint32 serial = KWaylandExtras::lastInputSerial(windowHandle());
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(KWaylandExtras::self(), &KWaylandExtras::xdgActivationTokenArrived, this,
                              [serial, connection, finish](int arrived, const QString &token) {
                                  if (quint32(arrived) != serial) {
                                      return;
                                  }
                                  QObject::disconnect(*connection);
                                  finish(token);
                              });
        KWaylandExtras::requestXdgActivationToken(windowHandle(), serial, QStringLiteral(KYU_APP_ID));
#endif
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
    // F5: check for updates, as Refresh in any KDE application.
    if (event->matches(QKeySequence::Refresh)) {
        run(Kyu::CardView::Action::Check);
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
