#pragma once

#include <QObject>
#include <QTimer>

// Decides who runs kyu-check. Normally the systemd user timer
// (kyu-check.timer) does, and "Check now" starts kyu-check.service, so checks
// are logged in the journal and never overlap. Without a systemd user
// manager (or with KYU_NO_SYSTEMD=1) the tray runs kyu-check itself, hourly.
class CheckScheduler : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        Unknown,
        Systemd,
        Internal,
    };

    explicit CheckScheduler(QObject *parent = nullptr);

    void start();
    Mode mode() const
    {
        return m_mode;
    }

public Q_SLOTS:
    void checkNow();

private:
    void useInternalTimer();
    void runCheckBinary();
    static QString checkBinary();

    Mode m_mode = Mode::Unknown;
    QTimer m_timer;
};
