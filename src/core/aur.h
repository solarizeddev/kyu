#pragma once

#include "updatestate.h"

#include <optional>

namespace Kyu
{
// The AUR helper kyu-update runs after pacman, and its AUR-only update.
struct AurHelper {
    QString name; // "paru", "yay" or "pamac"
    QStringList command; // {"paru", "-Sua"}
};

// The helper for a program: paru and yay update the AUR only with -Sua;
// pamac with "update --aur" (its repo part has nothing left to do after
// pacman). nullopt for anything else.
std::optional<AurHelper> aurHelperFor(const QString &program);

// paru, else yay, else pamac with the AUR on and its update check on
// ("EnableAUR" and "CheckAURUpdates" in pamac.conf). nullopt: none, and Kyu
// leaves the AUR alone.
std::optional<AurHelper> findAurHelper(const QString &pamacConf = QStringLiteral("/etc/pamac.conf"));

// What Kyu's settings say (kyu.conf, [AUR]: "check=false" leaves the AUR
// alone; "helper=yay" picks one), else findAurHelper().
std::optional<AurHelper> configuredAurHelper(const QString &configFile, const QString &pamacConf = QStringLiteral("/etc/pamac.conf"));

// pacman -Qm: the installed packages from outside the repos, name → version.
std::optional<QHash<QString, QString>> foreignPackages(const QString &pacman, QString *error);

// What pacman holds back (IgnorePkg, and the packages of IgnoreGroup), as
// pacman-conf reports it: names or globs.
QStringList heldBack(const QString &pacmanConf, const QString &pacman);

// pacman's version order (alpm_pkg_vercmp: epoch, version, release):
// negative, 0 or positive.
int vercmp(const QString &a, const QString &b);

struct AurCheckOptions {
    QString pacman = QStringLiteral("pacman");
    QString pacmanConf = QStringLiteral("pacman-conf");
    QString rpc = QStringLiteral("https://aur.archlinux.org/rpc/v5/info");
    int timeoutMs = 60 * 1000;
};

// The installed packages from outside the repos (pacman -Qm) that the AUR
// has a newer version of, from the AUR's web API (it gets their names).
// Packages the AUR does not know (a repo dropped them) and those pacman holds
// back are left out. No build, no helper involved.
AurResult checkAur(const AurCheckOptions &options = {});
}
