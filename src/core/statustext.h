#pragma once

#include "updatestate.h"

#include <optional>

namespace Kyu
{
// Lines for the tray tooltip. States what happened and when; no adjectives.
//   12 updates: core 2 · extra 7 · flathub 3
//   Checked 14:05
//   pacman check failed at 14:05: ...; list from 13:05
QStringList statusLines(const std::optional<UpdateState> &state,
                        bool checking,
                        const QString &loadError,
                        const QDateTime &now);

// Only the "... check failed at ..." lines.
QStringList failureLines(const UpdateState &state, const QDateTime &now);

// "14:05" today, else a short date and time.
QString shortWhen(const QDateTime &dt, const QDateTime &now);
}
