#pragma once

#include <QString>

namespace Kyu::Paths
{
// $XDG_STATE_HOME/kyu (default ~/.local/state/kyu). Created on demand.
QString stateDir();
// The checker writes here; the tray reads it.
QString stateFile();
// Held by kyu-check while a check runs.
QString lockFile();
// $XDG_CACHE_HOME/kyu/checkup-db, passed to checkupdates as CHECKUPDATES_DB so
// the synced databases can be queried afterwards for repo names.
QString checkupDbDir();
// The planner's own copy of the package databases (never kyu-check's: the two
// may run at the same time): $XDG_CACHE_HOME/kyu/plan-db.
QString planDbDir();
// $XDG_CONFIG_HOME/kyu/kyu.conf (default ~/.config/kyu/kyu.conf): Kyu's few
// settings, an INI file.
QString configFile();
}
