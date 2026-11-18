#pragma once

#include <QString>
#include <QStringList>

namespace Kyu
{
// A Kyu program next to the running one (a build tree), else its name (PATH).
QString siblingProgram(const QString &name);

// kyu-update in Kyu's window (kyu-window), or in a terminal when the window
// is not built. activationToken: a click's Wayland activation token, so the
// window may take focus.
bool startUpdate(const QStringList &args, const QString &activationToken = QString());
}
