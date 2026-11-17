#pragma once

#include <QObject>
#include <QStringList>
#include <QElapsedTimer>
#include <QTimer>

class QFileSystemWatcher;

// Tells the tray when a check is due outside the hourly timer:
// - packages changed (pamac, plain pacman, Discover) or Flatpaks changed:
//   once pacman's lock is gone and things have been quiet for a moment;
// - the computer woke from sleep (the network needs a moment first);
// - the network came back (worth it when the last check failed).
class SystemWatcher : public QObject
{
    Q_OBJECT
public:
    struct Paths {
        QString pacmanLocal = QStringLiteral("/var/lib/pacman/local"); // a directory per installed package
        QString pacmanLock = QStringLiteral("/var/lib/pacman/db.lck");
        QStringList flatpakChanged; // each installation's ".changed" file (its directory is watched too)
    };
    static Paths defaultPaths();

    explicit SystemWatcher(const Paths &paths = defaultPaths(), int quietMs = 20000, QObject *parent = nullptr);

    // Subscribes to logind's sleep and NetworkManager's state on the system bus.
    void watchSystemBus();

Q_SIGNALS:
    void packagesChanged();
    void resumed();
    void networkUp();

private Q_SLOTS:
    void prepareForSleep(bool sleeping);
    void networkStateChanged(uint state);

private:
    void changed();
    void settle();
    void rewatch();

    Paths m_paths;
    QFileSystemWatcher *m_watcher;
    QTimer m_quiet;
    QElapsedTimer m_waitingForLock; // pacman's lock held since
    uint m_networkState = 0;
};
