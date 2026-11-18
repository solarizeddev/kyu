#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Kyu
{
// What can be cleaned up after an update, read without root (row 31 of
// WHAT-KYU-SHOULD-BE): never removed by Kyu on its own; kyu-update offers
// "[c] clean up", each part then confirmed on its own.
struct Housekeeping {
    QStringList orphans; // pacman -Qtdq: installed as dependencies, needed by nothing (protected ones left out)
    QStringList orphanRemovals; // what pacman removes for them: with -Rs, they and what only they need
    bool orphansRecursive = true; // -Rs; false: -R, the orphans alone (what only they need would take too much)
    QStringList cacheFiles; // what paccache -rk3 and -ruk0 would remove
    qint64 cacheBytes = 0;
    qint64 cacheFree = -1; // free space where the cache is, -1 unknown
    QStringList unusedFlatpaks; // runtimes and extensions no app uses

    // The cache is worth a line from 1 GiB, or a tenth of the free space
    // left (from 100 MiB): after most Manjaro updates the old versions alone
    // are a few hundred MiB.
    bool cacheWorthCleaning() const;
    bool any() const
    {
        return !orphans.isEmpty() || cacheWorthCleaning() || !unusedFlatpaks.isEmpty();
    }
};

constexpr int CacheVersionsKept = 3;
// -Rs takes at most this many packages, and at most 3 for each orphan;
// beyond that only the orphans themselves are offered (-R).
constexpr int MaxOrphanRemovals = 30;

// Never offered as an orphan, whatever its install reason says (a container
// image or an installer may have marked it a dependency): base and
// base-devel, pacman's HoldPkg, kernels, microcode, firmware, keyrings, boot
// loaders and Secure Boot tools, init, disk and network essentials, the
// distribution's own system packages, and Kyu. Removing "base" with -Rs
// takes iproute2, procps-ng and 40 more with it (seen in a Manjaro container).
bool protectedFromCleanup(const QString &name, const QStringList &holdPkg);

// The files paccache lists with -vv in a dry run (one absolute path per
// line); its other lines ("==> finished dry run …", "no candidate packages
// found") left out, so its wording and language do not matter.
QStringList paccacheFiles(const QByteArray &output);

// Read now: orphans (pacman), the cache (paccache, when installed), unused
// Flatpaks (libflatpak, when built with it).
Housekeeping readHousekeeping();
}
