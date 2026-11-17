#include "statustext.h"

#include <QLocale>

namespace Kyu
{
namespace
{
QString when(const QDateTime &dt, const QDateTime &now)
{
    const QDateTime local = dt.toLocalTime();
    const QLocale locale;
    if (local.date() == now.toLocalTime().date()) {
        return locale.toString(local.time(), QLocale::ShortFormat);
    }
    return locale.toString(local, QLocale::ShortFormat);
}

void addFailure(QStringList &lines, const QString &source, const SourceStatus &status, const QDateTime &now)
{
    if (!status.checked || status.ok) {
        return;
    }
    const QString firstLine = status.error.section(QLatin1Char('\n'), 0, 0);
    QString line = QStringLiteral("%1 check failed at %2: %3").arg(source, when(status.checkedAt, now), firstLine);
    if (status.lastSuccessAt.isValid()) {
        line += QStringLiteral("; list from %1").arg(when(status.lastSuccessAt, now));
    }
    lines.append(line);
}
}

QStringList failureLines(const UpdateState &state, const QDateTime &now)
{
    QStringList lines;
    addFailure(lines, QStringLiteral("pacman"), state.pacman, now);
    addFailure(lines, QStringLiteral("Flatpak"), state.flatpak, now);
    return lines;
}

QString shortWhen(const QDateTime &dt, const QDateTime &now)
{
    return when(dt, now);
}

QStringList statusLines(const std::optional<UpdateState> &state,
                        bool checking,
                        const QString &loadError,
                        const QDateTime &now)
{
    QStringList lines;
    if (!loadError.isEmpty()) {
        lines.append(QStringLiteral("Cannot read the update list: %1").arg(loadError));
    }
    if (!state) {
        lines.append(checking ? QStringLiteral("Checking for updates") : QStringLiteral("Not checked yet"));
        return lines;
    }
    lines.append(state->summary());
    if (checking) {
        lines.append(QStringLiteral("Checking for updates"));
    } else if (state->checkedAt.isValid()) {
        lines.append(QStringLiteral("Checked %1").arg(when(state->checkedAt, now)));
    }
    addFailure(lines, QStringLiteral("pacman"), state->pacman, now);
    addFailure(lines, QStringLiteral("Flatpak"), state->flatpak, now);
    // The AUR's site is down now and then: said only after a day without it.
    const SourceStatus &aur = state->aur;
    if (aur.checked && !aur.ok && (!aur.lastSuccessAt.isValid() || aur.lastSuccessAt.secsTo(now) > 24 * 3600)) {
        lines.append(aur.lastSuccessAt.isValid() ? QStringLiteral("AUR not checked since %1: %2").arg(when(aur.lastSuccessAt, now), aur.error.section(QLatin1Char('\n'), 0, 0))
                                                 : QStringLiteral("AUR check failed: %1").arg(aur.error.section(QLatin1Char('\n'), 0, 0)));
    }
    return lines;
}
}
