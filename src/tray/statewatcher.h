#pragma once

#include "updatestate.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

#include <optional>

// Watches the state directory and reloads the state whenever kyu-check
// replaces the file or takes or releases its lock. Watching the directory
// rather than the file survives the atomic rename QSaveFile does.
class StateWatcher : public QObject
{
    Q_OBJECT
public:
    explicit StateWatcher(QObject *parent = nullptr);

    const std::optional<Kyu::UpdateState> &state() const
    {
        return m_state;
    }
    // Set when the file exists but could not be read.
    QString loadError() const
    {
        return m_loadError;
    }
    bool checking() const
    {
        return m_checking;
    }

public Q_SLOTS:
    void reload();

Q_SIGNALS:
    void changed();

private:
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    std::optional<Kyu::UpdateState> m_state;
    QString m_loadError;
    bool m_checking = false;
};
