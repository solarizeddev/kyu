#include "keyrings.h"

#include <QDir>
#include <QFile>

namespace Kyu
{
namespace
{
// The lines after a "%SECTION%" line of a local database file, up to the
// next empty line.
QStringList section(const QString &path, QLatin1StringView name)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QStringList lines;
    bool in = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (in) {
            if (line.isEmpty()) {
                break;
            }
            lines.append(line);
        } else if (line == name) {
            in = true;
        }
    }
    return lines;
}
}

QStringList installedKeyrings(const QString &localDb)
{
    QStringList keyrings;
    // A directory per package: name-version-release. Only names with
    // "keyring" in them are read further.
    const QDir dir(localDb);
    for (const QString &entry : dir.entryList({QStringLiteral("*keyring*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QStringList name = section(dir.filePath(entry + QStringLiteral("/desc")), QLatin1StringView("%NAME%"));
        if (name.isEmpty() || keyrings.contains(name.constFirst())) {
            continue;
        }
        const QStringList files = section(dir.filePath(entry + QStringLiteral("/files")), QLatin1StringView("%FILES%"));
        for (const QString &file : files) {
            if (file.startsWith(QLatin1String("usr/share/pacman/keyrings/")) && !file.endsWith(QLatin1Char('/'))) {
                keyrings.append(name.constFirst());
                break;
            }
        }
    }
    keyrings.sort();
    return keyrings;
}
}
