#pragma once

#include "updatestate.h"

namespace Kyu
{
struct PacmanCheckOptions {
    QString checkupdates = QStringLiteral("checkupdates");
    QString pacman = QStringLiteral("pacman");
    QString dbPath; // CHECKUPDATES_DB; empty means Paths::checkupDbDir()
    int timeoutMs = 5 * 60 * 1000;
};

// Runs checkupdates (no root, syncs into a private database copy, so no partial
// sync of the system database), then looks up each package's repo in that copy.
// A failed repo lookup is not an error: the packages are kept with no repo.
PacmanResult checkPacman(const PacmanCheckOptions &options = {});
}
