#pragma once

#include "updatestate.h"

namespace Kyu
{
// Lists Flatpak refs with updates in every system installation and the user
// installation, through libflatpak. Needs network access to the remotes.
// Without libflatpak at build time, returns available = false.
FlatpakResult checkFlatpak();
}
