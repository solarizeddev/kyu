#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// What an update leaves to do, read without root from /proc,
// /usr/lib/modules and pacman's log (ported from the old root helper).
namespace Kyu
{
struct KernelState {
    bool known = false; // false: no modules directory to compare with (a container)
    QString running; // "6.18.20-1-MANJARO"
    QString installed; // the running kernel package's installed version, when it can be told
    bool replaced = false; // the running kernel's modules are gone: restart to load new modules
};

// The running kernel against /usr/lib/modules.
KernelState kernelState(const QString &procDir = QStringLiteral("/proc"), const QString &modulesDir = QStringLiteral("/usr/lib/modules"));

struct OldLibraryProcess {
    int pid = 0;
    QString name;
};

// Processes of `uid` that still map a shared library the update replaced
// ("(deleted)" in /proc/PID/maps). Other users' maps are not readable
// without root, so only your own programs can be listed.
QList<OldLibraryProcess> processesOnReplacedLibraries(uint uid, const QString &procDir = QStringLiteral("/proc"));

// The path in one /proc/PID/maps line if it is a replaced shared library.
QString replacedLibrary(const QString &mapsLine);

// What one pacman run did, from the lines it appended to pacman.log.
struct PacmanRun {
    bool transactionStarted = false; // "[ALPM] transaction started"
    bool transactionCompleted = false;
    QStringList changed; // packages upgraded, installed, removed or downgraded
    QStringList upgraded; // names only
    QStringList configs; // "/etc/x.pacnew", "/etc/y.pacsave"
};

PacmanRun parsePacmanLog(const QString &appendedText);

// Whether the run replaced something only a restart brings into use, besides
// the kernel: systemd (PID 1 cannot be inspected without root) or glibc.
QString restartPackage(const PacmanRun &run);
}
