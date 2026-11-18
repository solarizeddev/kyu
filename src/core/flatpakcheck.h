#pragma once

#include "updatestate.h"

namespace Kyu
{
// Lists Flatpak refs with updates in every system installation and the user
// installation, through libflatpak. Needs network access to the remotes.
// Without libflatpak at build time, returns available = false.
// Gives up after timeoutSeconds (a remote that does not answer must not
// hold up every later check).
FlatpakResult checkFlatpak(int timeoutSeconds = 120);

// Runtimes and extensions no installed app uses, as `flatpak uninstall
// --unused` would remove them (pinned and still-needed ones kept), in every
// installation in use: "org.kde.Platform//6.8". Empty without libflatpak.
QStringList unusedFlatpakRefs();
}
