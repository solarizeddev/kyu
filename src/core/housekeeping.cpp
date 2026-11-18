#include "housekeeping.h"

#include "flatpakcheck.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>

namespace Kyu
{
namespace
{
QByteArray run(const QString &program, const QStringList &args)
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(env);
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(program, args);
    if (!process.waitForFinished(60000) || process.exitStatus() != QProcess::NormalExit) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    return process.readAllStandardOutput();
}
}

bool protectedFromCleanup(const QString &name, const QStringList &holdPkg)
{
    static const QRegularExpression system(QStringLiteral("^(base|base-devel|filesystem|glibc|pacman|pacman-mirrors|sudo|linux.*|.*-keyring|.*-firmware|"
                                                          "manjaro-.*|systemd.*|grub.*|mkinitcpio.*|dracut|efibootmgr|limine.*|refind)$"));
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
    for (const QByteArray &name : run(QStringLiteral("pacman-conf"), {QStringLiteral("HoldPkg")}).split('\n')) {
        hold.append(QString::fromUtf8(name.trimmed()));
    }
    // -Qtdq exits 1 when there are none.
    for (const QByteArray &line : run(QStringLiteral("pacman"), {QStringLiteral("-Qtdq")}).split('\n')) {
        const QString name = QString::fromUtf8(line.trimmed());
        if (!name.isEmpty() && !protectedFromCleanup(name, hold)) {
            h.orphans.append(name);
        }
    }
    // What -Rs would remove with them, as pacman works it out (no root
    // needed for --print): said before anything is asked.
    if (!h.orphans.isEmpty()) {
        for (const QByteArray &line : run(QStringLiteral("pacman"), QStringList{QStringLiteral("-Rs"), QStringLiteral("--print"), QStringLiteral("--print-format"),
                                                                                 QStringLiteral("%n")}
                                                                         + h.orphans)
                                          .split('\n')) {
            const QString name = QString::fromUtf8(line.trimmed());
            if (!name.isEmpty() && !name.contains(QLatin1Char(' '))) {
                h.orphanRemovals.append(name);
            }
        }
        // Anything protected among them: offered no more (pacman would take it).
        for (const QString &name : std::as_const(h.orphanRemovals)) {
            if (protectedFromCleanup(name, hold)) {
                h.orphans.clear();
                h.orphanRemovals.clear();
                break;
            }
        }
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("paccache")).isEmpty()) {
        // As kyu-update runs it: the newest 3 of each kept, then every version
        // of packages no longer installed.
        h.cacheFiles = paccacheFiles(run(QStringLiteral("paccache"), {QStringLiteral("-d"), QStringLiteral("-v"), QStringLiteral("-v"), QStringLiteral("-k"),
                                                                       QString::number(CacheVersionsKept)}))
            + paccacheFiles(run(QStringLiteral("paccache"), {QStringLiteral("-d"), QStringLiteral("-v"), QStringLiteral("-v"), QStringLiteral("-u"), QStringLiteral("-k"),
                                                             QStringLiteral("0")}));
        h.cacheFiles.removeDuplicates();
        for (const QString &file : std::as_const(h.cacheFiles)) {
            h.cacheBytes += QFileInfo(file).size();
        }
        if (!h.cacheFiles.isEmpty()) {
            const QStorageInfo storage(QFileInfo(h.cacheFiles.constFirst()).absolutePath());
            h.cacheFree = storage.isValid() ? storage.bytesAvailable() : -1;
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
