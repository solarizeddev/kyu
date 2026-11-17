#include "failuretext.h"

#include <QRegularExpression>

#include "keyrings.h"

#include <QFile>
#include <QJsonArray>

namespace
{
QStringList strings(const QJsonValue &v)
{
    QStringList list;
    for (const auto &item : v.toArray()) {
        list.append(item.toString());
    }
    return list;
}
}

QString distroId()
{
    QFile file(QStringLiteral("/etc/os-release"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1String("ID="))) {
            return line.mid(3).remove(QLatin1Char('"'));
        }
    }
    return {};
}

FailureText describeFailure(const QJsonObject &failure)
{
    const QString kind = failure.value(QStringLiteral("kind")).toString();
    const QStringList hosts = strings(failure.value(QStringLiteral("hosts")));
    FailureText t;
    if (kind == QLatin1String("network")) {
        t.title = QStringLiteral("No connection to the mirrors");
        t.text = QStringLiteral("pacman could not reach %1. Check the network connection, then update again.")
                     .arg(hosts.isEmpty() ? QStringLiteral("the mirrors") : hosts.join(QStringLiteral(", ")));
        t.items = hosts;
    } else if (kind == QLatin1String("mirrorlists")) {
        t.title = QStringLiteral("The mirror does not have the package lists");
        t.text = QStringLiteral("%1 answered \"404 Not Found\" for the package lists themselves: the mirror's address is wrong, or "
                                "the mirror is gone. Mirrors are listed in /etc/pacman.d/mirrorlist.")
                     .arg(hosts.isEmpty() ? QStringLiteral("The mirror") : hosts.join(QStringLiteral(", ")));
        t.items = strings(failure.value(QStringLiteral("files")));
        // Manjaro's own mirror list only for Manjaro's repositories (not a
        // third-party one).
        if (distroId() == QLatin1String("manjaro") && t.items.contains(QStringLiteral("core.db"))) {
            t.command = QStringLiteral("sudo pacman-mirrors --fasttrack && sudo pacman -Syyu");
            t.fix = QStringLiteral("sudo pacman-mirrors --fasttrack");
            t.text.replace(QStringLiteral(" Mirrors are listed in /etc/pacman.d/mirrorlist."), QStringLiteral(" A new mirror list fixes it."));
        }
    } else if (kind == QLatin1String("mirror")) {
        const QStringList files = strings(failure.value(QStringLiteral("files")));
        t.title = QStringLiteral("A mirror is out of date");
        t.text = QStringLiteral("%1 had the new package lists but not %2 (404 Not Found). Mirrors copy the main repositories with a "
                                "delay. Try again later, or use other mirrors.")
                     .arg(hosts.isEmpty() ? QStringLiteral("The mirror") : hosts.join(QStringLiteral(", ")),
                          files.size() == 1 ? QStringLiteral("the package file below") : QStringLiteral("%1 package files").arg(files.size()));
        t.items = files;
        // Manjaro's own mirror list only for Manjaro's repositories (not a
        // third-party one).
        if (distroId() == QLatin1String("manjaro") && t.items.contains(QStringLiteral("core.db"))) {
            t.command = QStringLiteral("sudo pacman-mirrors --fasttrack && sudo pacman -Syyu");
            t.fix = QStringLiteral("sudo pacman-mirrors --fasttrack");
        } else {
            t.text += QStringLiteral(" Mirrors are listed in /etc/pacman.d/mirrorlist.");
        }
    } else if (kind == QLatin1String("keyring")) {
        const QStringList packages = strings(failure.value(QStringLiteral("packages")));
        t.title = QStringLiteral("Package signatures could not be checked");
        t.text = QStringLiteral("pacman does not trust the key that signed %1. Usually the keyring is older than the packages: "
                                "updating the keyring first, then everything else, fixes it.")
                     .arg(packages.isEmpty() ? QStringLiteral("some packages") : packages.join(QStringLiteral(", ")));
        t.items = packages;
        QStringList keyrings = failure.contains(QStringLiteral("keyrings")) ? strings(failure.value(QStringLiteral("keyrings"))) : Kyu::installedKeyrings();
        if (keyrings.isEmpty()) {
            keyrings = {QStringLiteral("archlinux-keyring")};
            if (distroId() == QLatin1String("manjaro")) {
                keyrings.append(QStringLiteral("manjaro-keyring"));
            }
        }
        // Not a partial upgrade: -Su follows at once.
        t.command = QStringLiteral("sudo pacman -Sy --needed %1 && sudo pacman -Su").arg(keyrings.join(QLatin1Char(' ')));
        // kyu-update follows the fix with the whole update at once.
        t.fix = QStringLiteral("sudo pacman -Sy --needed %1").arg(keyrings.join(QLatin1Char(' ')));
    } else if (kind == QLatin1String("corrupt")) {
        const QStringList packages = strings(failure.value(QStringLiteral("packages")));
        const QStringList files = strings(failure.value(QStringLiteral("files")));
        t.title = QStringLiteral("A downloaded package is damaged");
        t.text = QStringLiteral("%1 in pacman's cache does not match its signature: the download was damaged or cut short, not the "
                                "keyring. If pacman deleted it (its question above), updating again downloads it anew; otherwise remove it "
                                "from /var/cache/pacman/pkg first.")
                     .arg(packages.isEmpty() ? QStringLiteral("A package") : packages.join(QStringLiteral(", ")));
        t.items = files;
        // The file names come from pacman's output (and from the mirror's
        // package list): a fix runs as root only for plain cache paths,
        // each quoted.
        static const QRegularExpression cached(QStringLiteral("^/var/cache/pacman/pkg/[A-Za-z0-9@._+:-]+$"));
        QStringList safe;
        for (const QString &f : files) {
            if (cached.match(f).hasMatch()) {
                safe.append(QLatin1Char('\'') + f + QLatin1Char('\''));
            }
        }
        if (!safe.isEmpty() && safe.size() == files.size()) {
            t.command = QStringLiteral("sudo rm -f %1").arg(safe.join(QLatin1Char(' ')));
            t.fix = t.command;
        }
    } else if (kind == QLatin1String("files")) {
        const QStringList files = strings(failure.value(QStringLiteral("files")));
        const int count = failure.value(QStringLiteral("fileCount")).toInt();
        t.title = QStringLiteral("Files already exist");
        t.text = QStringLiteral("%1 the update would install %2 already on disk, and no installed package owns %3. pacman does not "
                                "overwrite such files. Find out where they came from (pacman -Qo shows a file's package) before "
                                "removing or moving them.")
                     .arg(count == 1 ? QStringLiteral("One file") : QStringLiteral("%1 files").arg(count),
                          count == 1 ? QStringLiteral("is") : QStringLiteral("are"),
                          count == 1 ? QStringLiteral("it") : QStringLiteral("them"));
        t.items = files;
    } else if (kind == QLatin1String("disk")) {
        t.title = QStringLiteral("Not enough disk space");
        t.text = QStringLiteral("pacman needs more free space than there is (in pacman's messages). Removing old package versions from "
                                "pacman's cache often frees enough.");
        t.command = QStringLiteral("sudo paccache -r");
        t.fix = t.command;
    } else if (kind == QLatin1String("dependencies")) {
        t.title = QStringLiteral("Dependencies cannot be satisfied");
        t.text = QStringLiteral("pacman cannot resolve the update's dependencies (in pacman's messages). This often involves a package from "
                                "outside the repos, such as the AUR, or a package held back with IgnorePkg.");
    } else if (kind == QLatin1String("lock")) {
        t.title = QStringLiteral("pacman's database is locked");
        t.text = QStringLiteral("Another package manager was using pacman's database.");
    }
    return t;
}

QString failureTitle(const QString &kind)
{
    if (kind == QLatin1String("keyring")) {
        return QStringLiteral("Package signatures could not be checked");
    }
    // The other kinds look nothing up.
    return describeFailure(QJsonObject{{QStringLiteral("kind"), kind}}).title;
}
