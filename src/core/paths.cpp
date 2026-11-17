#include "paths.h"

#include <QDir>

namespace
{
QString xdgDir(const char *variable, const QString &fallback)
{
    const QString value = qEnvironmentVariable(variable);
    // The XDG spec says relative paths are invalid and must be ignored.
    if (!value.isEmpty() && QDir::isAbsolutePath(value)) {
        return value;
    }
    return QDir::homePath() + fallback;
}
}

namespace Kyu::Paths
{
QString stateDir()
{
    // QStandardPaths::GenericStateLocation needs Qt 6.7, so resolve it by hand.
    return xdgDir("XDG_STATE_HOME", QStringLiteral("/.local/state")) + QStringLiteral("/kyu");
}

QString stateFile()
{
    return stateDir() + QStringLiteral("/updates.json");
}

QString lockFile()
{
    return stateDir() + QStringLiteral("/check.lock");
}

QString configFile()
{
    return xdgDir("XDG_CONFIG_HOME", QStringLiteral("/.config")) + QStringLiteral("/kyu/kyu.conf");
}

QString checkupDbDir()
{
    return xdgDir("XDG_CACHE_HOME", QStringLiteral("/.cache")) + QStringLiteral("/kyu/checkup-db");
}
}
