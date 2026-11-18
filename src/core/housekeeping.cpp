#include "housekeeping.h"

#include "flatpakcheck.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>

#include <optional>

namespace Kyu
{
namespace
{
// Its output when it ran and exited 0 within 10 s (else nothing to offer).
std::optional<QByteArray> run(const QString &program, const QStringList &args, bool zeroOnly = true)
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(env);
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(program, args);
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit) {
        process.kill();
        process.waitForFinished(1000);
        return std::nullopt;
    }
    if (zeroOnly && process.exitCode() != 0) {
        return std::nullopt;
    }
    return process.readAllStandardOutput();
}
}

bool protectedFromCleanup(const QString &name, const QStringList &holdPkg)
{
    static const QRegularExpression system(QStringLiteral("^(base|base-devel|filesystem|glibc|pacman|pacman-mirrors|sudo|linux.*|.*-ucode|.*-keyring|.*-firmware|"
                                                          "manjaro-.*|systemd.*|grub.*|mkinitcpio.*|dracut|booster|efibootmgr|limine.*|refind|sbctl|shim.*|"
                                                          "cryptsetup|lvm2|mdadm|btrfs-progs|e2fsprogs|dosfstools|networkmanager|kyu.*)$"));
    return holdPkg.contains(name) || system.match(name).hasMatch();
}

bool Housekeeping::cacheWorthCleaning() const
{
    constexpr qint64 MiB = 1024 * 1024;
    if (cacheBytes >= 1024 * MiB) {
        return true;
    }
    return cacheBytes >= 100 * MiB && cacheFree >= 0 && cacheBytes * 10 >= cacheFree;
}

QStringList paccacheFiles(const QByteArray &output)
{
    QStringList files;
    for (const QByteArray &line : output.split('\n')) {
        const QString path = QString::fromLocal8Bit(line.trimmed());
        if (path.startsWith(QLatin1Char('/')) && path.contains(QLatin1String(".pkg.tar"))) {
            files.append(path);
        }
    }
    return files;
}

Housekeeping readHousekeeping()
{
    Housekeeping h;
    QStringList hold;
    for (const QByteArray &name : run(QStringLiteral("pacman-conf"), {QStringLiteral("HoldPkg")}).value_or(QByteArray()).split('\n')) {
        hold.append(QString::fromUtf8(name.trimmed()));
    }
    // -Qtdq exits 1 when there are none.
    for (const QByteArray &line : run(QStringLiteral("pacman"), {QStringLiteral("-Qtdq")}, false).value_or(QByteArray()).split('\n')) {
        const QString name = QString::fromUtf8(line.trimmed());
        if (!name.isEmpty() && !protectedFromCleanup(name, hold)) {
            h.orphans.append(name);
        }
    }
    // What pacman would remove with them, as it works it out (no root needed
    // for --print): said before anything is asked. Offered only when that
    // dry run answered, for every orphan: a failed one says nothing about
    // what -Rs would take.
    auto dryRun = [&h](const QString &mode) -> std::optional<QStringList> {
        const auto out = run(QStringLiteral("pacman"), QStringList{mode, QStringLiteral("--print"), QStringLiteral("--print-format"), QStringLiteral("%n")} + h.orphans);
        if (!out) {
            return std::nullopt;
        }
        QStringList names;
        for (const QByteArray &line : out->split('\n')) {
            const QString name = QString::fromUtf8(line.trimmed());
            if (!name.isEmpty() && !name.contains(QLatin1Char(' '))) {
                names.append(name);
            }
        }
        for (const QString &orphan : std::as_const(h.orphans)) {
            if (!names.contains(orphan)) {
                return std::nullopt;
            }
        }
        return names;
    };
    if (!h.orphans.isEmpty()) {
        std::optional<QStringList> removals = dryRun(QStringLiteral("-Rs"));
        // With what only they need: unless that takes something protected,
        // or far more than the orphans (a metapackage marked a dependency).
        const bool tooMuch = removals
            && (removals->size() > MaxOrphanRemovals || removals->size() > 3 * h.orphans.size()
                || std::any_of(removals->cbegin(), removals->cend(), [&hold](const QString &n) {
                       return protectedFromCleanup(n, hold);
                   }));
        if (tooMuch) {
            h.orphansRecursive = false;
            removals = dryRun(QStringLiteral("-R"));
        }
        if (removals) {
            h.orphanRemovals = *removals;
        } else {
            h.orphans.clear();
        }
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("paccache")).isEmpty()) {
        // As kyu-update runs it: the newest 3 of each kept, then every version
        // of packages no longer installed.
        // (paccache exits 1 when there is nothing to prune.)
        h.cacheFiles = paccacheFiles(run(QStringLiteral("paccache"), {QStringLiteral("-d"), QStringLiteral("-v"), QStringLiteral("-v"), QStringLiteral("-k"),
                                                                       QString::number(CacheVersionsKept)}, false).value_or(QByteArray()))
            + paccacheFiles(run(QStringLiteral("paccache"), {QStringLiteral("-d"), QStringLiteral("-v"), QStringLiteral("-v"), QStringLiteral("-u"), QStringLiteral("-k"),
                                                             QStringLiteral("0")}, false).value_or(QByteArray()));
        h.cacheFiles.removeDuplicates();
        for (const QString &file : std::as_const(h.cacheFiles)) {
            h.cacheBytes += QFileInfo(file).size();
        }
        // With several cache directories: the fullest one's free space.
        for (const QString &file : std::as_const(h.cacheFiles)) {
            const QStorageInfo storage(QFileInfo(file).absolutePath());
            if (storage.isValid() && (h.cacheFree < 0 || storage.bytesAvailable() < h.cacheFree)) {
                h.cacheFree = storage.bytesAvailable();
            }
        }
    }
    // Tests set the answer: the machine's own Flatpaks are not theirs.
    if (qEnvironmentVariableIsSet("KYU_TEST_UNUSED_FLATPAKS")) {
        h.unusedFlatpaks = qEnvironmentVariable("KYU_TEST_UNUSED_FLATPAKS").split(QLatin1Char(','), Qt::SkipEmptyParts);
    } else {
        h.unusedFlatpaks = unusedFlatpakRefs();
    }
    return h;
}
}
