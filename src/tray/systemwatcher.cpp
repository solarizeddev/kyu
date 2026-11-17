#include "systemwatcher.h"

#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>

namespace
{
// NetworkManager's NM_STATE_CONNECTED_GLOBAL: full internet access.
constexpr uint ConnectedGlobal = 70;
}

SystemWatcher::Paths SystemWatcher::defaultPaths()
{
    Paths paths;
    // Flatpak touches ".changed" in an installation whenever it changes.
    paths.flatpakChanged = {QStringLiteral("/var/lib/flatpak/.changed"), QDir::homePath() + QStringLiteral("/.local/share/flatpak/.changed")};
    return paths;
}

SystemWatcher::SystemWatcher(const Paths &paths, int quietMs, QObject *parent)
    : QObject(parent)
    , m_paths(paths)
    , m_watcher(new QFileSystemWatcher(this))
{
    m_quiet.setSingleShot(true);
    m_quiet.setInterval(quietMs);
    connect(&m_quiet, &QTimer::timeout, this, &SystemWatcher::settle);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &SystemWatcher::pathChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &SystemWatcher::pathChanged);
    rewatch();
    m_flatpakMarks = flatpakMarks();
}

void SystemWatcher::rewatch()
{
    QStringList wanted;
    if (QFileInfo(m_paths.pacmanLocal).isDir()) {
        wanted.append(m_paths.pacmanLocal);
    }
    for (const QString &file : std::as_const(m_paths.flatpakChanged)) {
        // The installation's directory too: a ".changed" that appears later
        // (the first Flatpak installed) or is renamed into place shows there.
        const QString dir = QFileInfo(file).absolutePath();
        if (QFileInfo(dir).isDir()) {
            wanted.append(dir);
        }
        if (QFileInfo::exists(file)) {
            wanted.append(file);
        }
    }
    // A file replaced by a new one (as .changed often is) drops out of the
    // watch: add whatever is missing again.
    const QStringList watched = m_watcher->files() + m_watcher->directories();
    for (const QString &path : std::as_const(wanted)) {
        if (!watched.contains(path)) {
            m_watcher->addPath(path);
        }
    }
}

QHash<QString, QDateTime> SystemWatcher::flatpakMarks() const
{
    QHash<QString, QDateTime> marks;
    for (const QString &file : m_paths.flatpakChanged) {
        const QFileInfo info(file);
        marks.insert(file, info.exists() ? info.lastModified() : QDateTime());
    }
    return marks;
}

void SystemWatcher::pathChanged(const QString &path)
{
    rewatch();
    // Flatpak: only its ".changed" mark counts, whatever else appears next
    // to it (so nothing a check writes there can start a check again).
    if (path != m_paths.pacmanLocal) {
        const QHash<QString, QDateTime> marks = flatpakMarks();
        if (marks == m_flatpakMarks) {
            return;
        }
        m_flatpakMarks = marks;
    }
    // A transaction changes many packages: wait until it has been quiet.
    m_quiet.start();
}

void SystemWatcher::settle()
{
    // Still running (pacman holds its lock): look again later, but not
    // forever (a lock left over from a crash would hold it).
    if (QFileInfo::exists(m_paths.pacmanLock)) {
        if (!m_waitingForLock.isValid()) {
            m_waitingForLock.start();
        }
        if (m_waitingForLock.elapsed() < 30 * 60 * 1000) {
            m_quiet.start();
            return;
        }
    }
    m_waitingForLock.invalidate();
    Q_EMIT packagesChanged();
}

void SystemWatcher::watchSystemBus()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return;
    }
    bus.connect(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"),
                QStringLiteral("PrepareForSleep"), this, SLOT(prepareForSleep(bool)));
    bus.connect(QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("/org/freedesktop/NetworkManager"),
                QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("StateChanged"), this, SLOT(networkStateChanged(uint)));
}

void SystemWatcher::prepareForSleep(bool sleeping)
{
    if (!sleeping) {
        // Give the network a minute to come back first.
        QTimer::singleShot(60000, this, &SystemWatcher::resumed);
    }
}

void SystemWatcher::networkStateChanged(uint state)
{
    // NetworkManager only reports changes, so a first "connected" is one too
    // (logged in without a network).
    const bool up = state == ConnectedGlobal && m_networkState != ConnectedGlobal;
    m_networkState = state;
    if (up) {
        Q_EMIT networkUp();
    }
}
