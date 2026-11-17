#pragma once

#include <QObject>
#include <QPointer>
#include <QWidget>

// Kyu's window on the session bus while it is open, so the card's "Show
// window" can bring it forward (with the card click's activation token, the
// focus Wayland allows).
class Activator : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.solarizeddev.kyu.Window")
public:
    explicit Activator(QWidget *window);

    // Registers the service; false if another Kyu window has it.
    bool publish();

public Q_SLOTS:
    // token: a Wayland activation token (may be empty).
    Q_SCRIPTABLE void Activate(const QString &token);

private:
    QPointer<QWidget> m_window;
};
