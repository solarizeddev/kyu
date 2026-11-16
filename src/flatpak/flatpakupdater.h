#pragma once

#include "jsonlog.h"

#include <QStringList>

// Updates every installed Flatpak ref that has an update, in every system
// installation and the user installation, through libflatpak. Writes
// progress to the log as it goes. Runs in the user's session: system
// installations go through flatpak's own system helper and polkit rules.
class FlatpakUpdater
{
public:
    struct Result {
        bool ok = true;
        int changed = 0;
        int total = 0;
        QStringList errors;
    };

    explicit FlatpakUpdater(Kyu::JsonLog &log);
    Result run();

    // libflatpak signal handlers (C callbacks forward here).
    void newOperation(void *operation);
    void operationDone(void *operation);
    bool operationError(void *operation, const QString &message, bool fatal);
    bool ready(void *transaction);
    void endOfLife(const QString &ref, const QString &reason, const QString &rebase);
    void output(const QString &text, const QString &stream = QStringLiteral("stdout"));

private:
    Kyu::JsonLog &m_log;
    Result m_result;
    int m_current = 0;
    int m_estimate = 0; // refs with updates; ops can add related refs
    int m_doneBefore = 0; // ops of earlier installations
    QString m_installation;
};
