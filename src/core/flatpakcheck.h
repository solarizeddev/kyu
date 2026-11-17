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
}
