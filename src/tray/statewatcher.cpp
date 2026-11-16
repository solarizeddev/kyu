#include "statewatcher.h"

#include "paths.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>

#include <csignal>
#include <cerrno>

namespace
{
bool checkRunning()
{
    // Read the lock without taking it: taking it would create the file,
    // which would fire the directory watcher again.
    QLockFile lock(Kyu::Paths::lockFile());
    qint64 pid = 0;
    QString host;
    QString appName;
    if (!lock.getLockInfo(&pid, &host, &appName) || pid <= 0) {
        return false;
    }
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}
}

StateWatcher::StateWatcher(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(Kyu::Paths::stateDir());
    m_watcher.addPath(Kyu::Paths::stateDir());

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_debounce, &QTimer::timeout, this, &StateWatcher::reload);
}

void StateWatcher::reload()
{
    m_checking = checkRunning();
    m_loadError.clear();
    const QString path = Kyu::Paths::stateFile();
    if (QFileInfo::exists(path)) {
        QString error;
        auto loaded = Kyu::UpdateState::load(path, &error);
        if (loaded) {
            m_state = std::move(loaded);
        } else {
            m_loadError = error;
        }
    } else {
        m_state.reset();
    }
    Q_EMIT changed();
}
