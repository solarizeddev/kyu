#include "failuretext.h"

#include <QRegularExpression>

#include "i18n.h"
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
    const QString hostList = hosts.join(QStringLiteral(", "));
    const QString mirrorList = QStringLiteral("/etc/pacman.d/mirrorlist");
    FailureText t;
    if (kind == QLatin1String("network")) {
        t.title = Kyu::I18n::tr("No connection to the mirrors");
        t.text = hosts.isEmpty() ? Kyu::I18n::tr("pacman could not reach the mirrors. Check the network connection, then update again.")
                                 : Kyu::I18n::tr("pacman could not reach %1. Check the network connection, then update again.").arg(hostList);
        t.items = hosts;
    } else if (kind == QLatin1String("mirrorlists")) {
        t.title = Kyu::I18n::tr("The mirror does not have the package lists");
        t.text = hosts.isEmpty() ? Kyu::I18n::tr("The mirror answered \"404 Not Found\" for the package lists themselves: the mirror's address is wrong, or "
                                            "the mirror is gone.")
                                 : Kyu::I18n::tr("%1 answered \"404 Not Found\" for the package lists themselves: the mirror's address is wrong, or "
                                            "the mirror is gone.")
                                       .arg(hostList);
        t.items = strings(failure.value(QStringLiteral("files")));
        // Manjaro's own mirror list only for Manjaro's repositories (not a
        // third-party one).
        if (distroId() == QLatin1String("manjaro") && t.items.contains(QStringLiteral("core.db"))) {
            t.command = QStringLiteral("sudo pacman-mirrors --fasttrack && sudo pacman -Syyu");
            t.fix = QStringLiteral("sudo pacman-mirrors --fasttrack");
            t.text += QLatin1Char(' ') + Kyu::I18n::tr("A new mirror list fixes it.");
        } else {
            t.text += QLatin1Char(' ') + Kyu::I18n::tr("Mirrors are listed in %1.").arg(mirrorList);
        }
    } else if (kind == QLatin1String("mirror")) {
        const QStringList files = strings(failure.value(QStringLiteral("files")));
        t.title = Kyu::I18n::tr("A mirror is out of date");
        t.text = (hosts.isEmpty() ? Kyu::numbered("The mirror had the new package lists but not %n package files (404 Not Found).",
                                                  "The mirror had the new package lists but not the package file below (404 Not Found).", files.size())
                                  : Kyu::numbered("%1 had the new package lists but not %n package files (404 Not Found).",
                                                  "%1 had the new package lists but not the package file below (404 Not Found).", files.size())
                                        .arg(hostList))
            + QLatin1Char(' ') + Kyu::I18n::tr("Mirrors copy the main repositories with a delay. Try again later, or use other mirrors.");
        t.items = files;
        // Manjaro's own mirror list only for Manjaro's repositories (not a
        // third-party one).
        if (distroId() == QLatin1String("manjaro") && t.items.contains(QStringLiteral("core.db"))) {
            t.command = QStringLiteral("sudo pacman-mirrors --fasttrack && sudo pacman -Syyu");
            t.fix = QStringLiteral("sudo pacman-mirrors --fasttrack");
        } else {
            t.text += QLatin1Char(' ') + Kyu::I18n::tr("Mirrors are listed in %1.").arg(mirrorList);
        }
    } else if (kind == QLatin1String("keyring")) {
        const QStringList packages = strings(failure.value(QStringLiteral("packages")));
        t.title = Kyu::I18n::tr("Package signatures could not be checked");
        t.text = (packages.isEmpty() ? Kyu::I18n::tr("pacman does not trust the key that signed some packages.")
                                     : Kyu::I18n::tr("pacman does not trust the key that signed %1.").arg(packages.join(QStringLiteral(", "))))
            + QLatin1Char(' ') + Kyu::I18n::tr("Usually the keyring is older than the packages: updating the keyring first, then everything else, fixes it.");
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
        t.title = Kyu::I18n::tr("A downloaded package is damaged");
        t.text = (packages.isEmpty() ? Kyu::I18n::tr("A package in pacman's cache does not match its signature: the download was damaged or cut short, "
                                                "not the keyring.")
                                     : Kyu::I18n::tr("%1 in pacman's cache does not match its signature: the download was damaged or cut short, not the "
                                                "keyring.")
                                           .arg(packages.join(QStringLiteral(", "))))
            + QLatin1Char(' ')
            + Kyu::I18n::tr("If pacman deleted it (its question above), updating again downloads it anew; otherwise remove it from %1 first.")
                  .arg(QStringLiteral("/var/cache/pacman/pkg"));
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
        t.title = Kyu::I18n::tr("Files already exist");
        t.text = Kyu::numbered("%n files the update would install are already on disk, and no installed package owns them.",
                               "One file the update would install is already on disk, and no installed package owns it.", count)
            + QLatin1Char(' ')
            + Kyu::I18n::tr("pacman does not overwrite such files. Find out where they came from (%1 shows a file's package) before removing or moving them.")
                  .arg(QStringLiteral("pacman -Qo"));
        t.items = files;
    } else if (kind == QLatin1String("disk")) {
        t.title = Kyu::I18n::tr("Not enough disk space");
        t.text = Kyu::I18n::tr("pacman needs more free space than there is (in pacman's messages). Removing old package versions from "
                          "pacman's cache often frees enough.");
        t.command = QStringLiteral("sudo paccache -r");
        t.fix = t.command;
    } else if (kind == QLatin1String("dependencies")) {
        t.title = Kyu::I18n::tr("Dependencies cannot be satisfied");
        t.text = Kyu::I18n::tr("pacman cannot resolve the update's dependencies (in pacman's messages). This often involves a package from "
                          "outside the repos, such as the AUR, or a package held back with IgnorePkg.");
    } else if (kind == QLatin1String("lock")) {
        t.title = Kyu::I18n::tr("pacman's database is locked");
        t.text = Kyu::I18n::tr("Another package manager was using pacman's database.");
    }
    return t;
}

QString failureTitle(const QString &kind)
{
    if (kind == QLatin1String("keyring")) {
        return Kyu::I18n::tr("Package signatures could not be checked");
    }
    // The other kinds look nothing up.
    return describeFailure(QJsonObject{{QStringLiteral("kind"), kind}}).title;
}
