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
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &SystemWatcher::changed);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &SystemWatcher::changed);
    rewatch();
}

void SystemWatcher::rewatch()
{
    QStringList wanted;
    if (QFileInfo(m_paths.pacmanLocal).isDir()) {
        wanted.append(m_paths.pacmanLocal);
    }
    for (const QString &file : std::as_const(m_paths.flatpakChanged)) {
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

void SystemWatcher::changed()
{
    rewatch();
    // A transaction changes many packages: wait until it has been quiet.
    m_quiet.start();
}

void SystemWatcher::settle()
{
    // Still running (pacman holds its lock): look again later.
    if (QFileInfo::exists(m_paths.pacmanLock)) {
        m_quiet.start();
        return;
    }
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
    const bool up = state == ConnectedGlobal && m_networkState != ConnectedGlobal && m_networkState != 0;
    m_networkState = state;
    if (up) {
        Q_EMIT networkUp();
    }
}
