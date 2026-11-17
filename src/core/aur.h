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

// paru, else yay, else pamac with the AUR enabled in its settings
// ("EnableAUR" in pamac.conf). nullopt: none, and Kyu leaves the AUR alone.
std::optional<AurHelper> findAurHelper(const QString &pamacConf = QStringLiteral("/etc/pamac.conf"));

// pacman's version order (alpm_pkg_vercmp: epoch, version, release):
// negative, 0 or positive.
int vercmp(const QString &a, const QString &b);

struct AurCheckOptions {
    QString pacman = QStringLiteral("pacman");
    QString rpc = QStringLiteral("https://aur.archlinux.org/rpc/v5/info");
    int timeoutMs = 60 * 1000;
};

// The installed packages from outside the repos (pacman -Qm) that the AUR
// has a newer version of, from the AUR's web API. Packages the AUR does not
// know (a repo dropped them) are left out. No build, no helper involved.
AurResult checkAur(const AurCheckOptions &options = {});
}
