#include "aftercare.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <sys/stat.h>

namespace Kyu
{
namespace
{
QString readTrimmed(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}
}

KernelState kernelState(const QString &procDir, const QString &modulesDir)
{
    KernelState k;
    k.running = readTrimmed(procDir + QStringLiteral("/sys/kernel/osrelease"));
    if (k.running.isEmpty()) {
        return k;
    }
    // Module directories with a kernel image in them (not leftovers).
    QStringList dirs;
    for (const QString &name : QDir(modulesDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFileInfo(modulesDir + QLatin1Char('/') + name + QStringLiteral("/vmlinuz")).isFile()) {
            dirs.append(name);
        }
    }
    if (dirs.isEmpty()) {
        return k;
    }
    k.known = true;
    if (QFileInfo(modulesDir + QLatin1Char('/') + k.running).isDir()) {
        k.installed = k.running;
        return k;
    }
    k.replaced = true;
    // Which directory is the running kernel's package now: BOOT_IMAGE=/boot/vmlinuz-<pkgbase>.
    QString pkgbase;
    for (const QString &arg : readTrimmed(procDir + QStringLiteral("/cmdline")).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (arg.startsWith(QLatin1String("BOOT_IMAGE="))) {
            const QString file = arg.section(QLatin1Char('/'), -1);
            if (file.startsWith(QLatin1String("vmlinuz-"))) {
                pkgbase = file.mid(8);
            }
        }
    }
    if (!pkgbase.isEmpty()) {
        for (const QString &dir : dirs) {
            if (readTrimmed(modulesDir + QLatin1Char('/') + dir + QStringLiteral("/pkgbase")) == pkgbase) {
                k.installed = dir;
            }
        }
    } else if (dirs.size() == 1) {
        k.installed = dirs.first();
    }
    return k;
}

QString replacedLibrary(const QString &mapsLine)
{
    // address perms offset dev inode pathname; the pathname may contain spaces.
    static const QRegularExpression fields(QStringLiteral("^\\S+\\s+\\S+\\s+\\S+\\s+\\S+\\s+\\S+\\s+(.*)$"));
    const auto match = fields.match(mapsLine);
    if (!match.hasMatch()) {
        return {};
    }
    QString path = match.captured(1).trimmed();
    if (!path.endsWith(QLatin1String(" (deleted)"))) {
        return {};
    }
    path.chop(10);
    const bool inLib = path.startsWith(QLatin1String("/usr/lib/")) || path.startsWith(QLatin1String("/usr/lib64/"))
        || path.startsWith(QLatin1String("/lib/")) || path.startsWith(QLatin1String("/lib64/"));
    const QString file = path.section(QLatin1Char('/'), -1);
    const bool shared = file.endsWith(QLatin1String(".so")) || file.contains(QLatin1String(".so."));
    return inLib && shared ? path : QString();
}

QList<OldLibraryProcess> processesOnReplacedLibraries(uint uid, const QString &procDir)
{
    QList<OldLibraryProcess> found;
    for (const QString &entry : QDir(procDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const int pid = entry.toInt(&isPid);
        if (!isPid) {
            continue;
        }
        const QString dir = procDir + QLatin1Char('/') + entry;
        struct stat st {};
        if (::stat(QFile::encodeName(dir).constData(), &st) != 0 || st.st_uid != uid) {
            continue;
        }
        QFile maps(dir + QStringLiteral("/maps"));
        if (!maps.open(QIODevice::ReadOnly)) {
            continue; // exited, or not readable
        }
        bool replaced = false;
        while (!maps.atEnd() && !replaced) {
            replaced = !replacedLibrary(QString::fromUtf8(maps.readLine())).isEmpty();
        }
        if (replaced) {
            const QString name = readTrimmed(dir + QStringLiteral("/comm"));
            found.append({pid, name.isEmpty() ? entry : name});
        }
    }
    std::sort(found.begin(), found.end(), [](const OldLibraryProcess &a, const OldLibraryProcess &b) {
        return a.name == b.name ? a.pid < b.pid : a.name < b.name;
    });
    return found;
}

PacmanRun parsePacmanLog(const QString &appendedText)
{
    // [2026-11-16T14:10:23+0100] [ALPM] upgraded linux618 (6.18.20-1 -> 6.18.22-1)
    // [2026-11-16T14:10:23+0100] [ALPM] warning: /etc/sddm.conf installed as /etc/sddm.conf.pacnew
    static const QRegularExpression change(QStringLiteral("^\\[[^]]*\\] \\[ALPM\\] (upgraded|installed|removed|downgraded|reinstalled) (\\S+) "));
    static const QRegularExpression config(QStringLiteral("^\\[[^]]*\\] \\[ALPM\\] warning: \\S+ (?:installed|saved) as (\\S+\\.pac(?:new|save))\\s*$"));
    PacmanRun run;
    for (const QString &line : appendedText.split(QLatin1Char('\n'))) {
        if (line.contains(QLatin1String("] [ALPM] transaction started"))) {
            run.transactionStarted = true;
        } else if (line.contains(QLatin1String("] [ALPM] transaction completed"))) {
            run.transactionCompleted = true;
        } else if (const auto m = change.match(line); m.hasMatch()) {
            run.changed.append(m.captured(2));
            if (m.captured(1) == QLatin1String("upgraded")) {
                run.upgraded.append(m.captured(2));
            }
        } else if (const auto c = config.match(line); c.hasMatch()) {
            if (!run.configs.contains(c.captured(1))) {
                run.configs.append(c.captured(1));
            }
        }
    }
    return run;
}

QString restartPackage(const PacmanRun &run)
{
    for (const QString &name : {QStringLiteral("systemd"), QStringLiteral("systemd-libs"), QStringLiteral("glibc")}) {
        if (run.upgraded.contains(name)) {
            return name;
        }
    }
    return {};
}
}
