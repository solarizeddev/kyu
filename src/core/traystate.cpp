#include "traystate.h"

#include "statustext.h"

namespace Kyu
{
TrayView trayView(const TrayInput &in)
{
    TrayView v;
    v.passive = false;
    const QJsonObject &last = in.lastRun;
    const QDateTime lastTime = QDateTime::fromString(last.value(QStringLiteral("time")).toString(), Qt::ISODate);
    // A failed update stays until a later check finds the system up to date.
    const bool lastFailed = last.value(QStringLiteral("pacmanExit")).toInt(0) > 0
        && !(in.state && in.state->checkedAt.isValid() && lastTime.isValid() && in.state->checkedAt > lastTime && in.state->total() == 0);
    const bool checkFailed = !in.loadError.isEmpty() || (in.state && in.state->hasError());
    const bool restart = in.kernelReplaced || (in.sameBoot && last.value(QStringLiteral("restartRequired")).toBool());
    const int total = in.state ? in.state->total() : 0;
    const bool news = in.state && total > 0 && !in.state->news.items.isEmpty();
    const QStringList status = statusLines(in.state, in.checking, in.loadError, in.now);

    if (in.updating) {
        v.mark = TrayMark::Updating;
        v.tooltip = QStringLiteral("Updating");
    } else if (lastFailed) {
        v.mark = TrayMark::Error;
        v.iconName = QStringLiteral("update-high");
        v.tooltip = QStringLiteral("The last update did not finish");
    } else if (checkFailed) {
        v.mark = TrayMark::Error;
        v.iconName = QStringLiteral("update-high");
        v.tooltip = status.join(QLatin1Char('\n'));
    } else if (restart) {
        v.mark = TrayMark::Restart;
        v.iconName = QStringLiteral("system-reboot-update");
        const QString reason = last.value(QStringLiteral("restartReason")).toString();
        v.tooltip = QStringLiteral("Restart to finish") + (in.sameBoot && !reason.isEmpty() ? QStringLiteral(": ") + reason : QString());
    } else if (news) {
        v.mark = TrayMark::News;
        v.iconName = QStringLiteral("update-medium");
        v.tooltip = QStringLiteral("Read first: ") + in.state->news.items.constFirst().title + QLatin1Char('\n') + status.value(0);
    } else if (total > 0) {
        v.mark = TrayMark::Updates;
        v.iconName = QStringLiteral("update-low");
        v.tooltip = status.join(QLatin1Char('\n'));
    } else {
        v.mark = TrayMark::UpToDate;
        v.iconName = QStringLiteral("update-none");
        v.tooltip = status.join(QLatin1Char('\n'));
        v.passive = !in.checking;
    }
    return v;
}
}
