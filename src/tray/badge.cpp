#include "badge.h"

#include <QPainter>
#include <QPainterPath>

namespace
{
QIcon draw(const QIcon &base, int count, const QPalette &palette, bool overlay)
{
    const QString text = count > 99 ? QStringLiteral("99+") : QString::number(count);

    QIcon result;
    for (int size : {16, 22, 24, 32, 48, 64}) {
        // QIcon::pixmap() may return a smaller pixmap than asked for (it never
        // upscales), so draw onto a canvas of exactly the requested size.
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QPixmap source = base.pixmap(QSize(size, size));
        source.setDevicePixelRatio(1.0);
        if (!source.isNull()) {
            painter.drawPixmap((size - source.width()) / 2, (size - source.height()) / 2, source);
        }

        QFont font = painter.font();
        font.setBold(true);
        const int badgeHeight = overlay ? size : qMax(9, qRound(size * 0.55));
        font.setPixelSize(qMax(7, qRound(badgeHeight * 0.75)));
        painter.setFont(font);

        const int textWidth = painter.fontMetrics().horizontalAdvance(text);
        const int badgeWidth = qMin(size, qMax(badgeHeight, textWidth + badgeHeight / 2));
        const QRectF rect(size - badgeWidth, size - badgeHeight, badgeWidth, badgeHeight);

        QPainterPath path;
        path.addRoundedRect(rect, badgeHeight / 2.0, badgeHeight / 2.0);
        painter.fillPath(path, palette.color(QPalette::Active, QPalette::Highlight));
        painter.setPen(palette.color(QPalette::Active, QPalette::HighlightedText));
        painter.drawText(rect, Qt::AlignCenter, text);
        painter.end();

        result.addPixmap(pixmap);
    }
    return result;
}
}

QIcon badgedIcon(const QIcon &base, int count, const QPalette &palette)
{
    return count > 0 ? draw(base, count, palette, false) : base;
}

QIcon countOverlay(int count, const QPalette &palette)
{
    return count > 0 ? draw(QIcon(), count, palette, true) : QIcon();
}
