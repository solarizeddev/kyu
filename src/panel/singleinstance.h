#pragma once

#include <QObject>

// One panel per session: a second start asks the first one to show itself
// (over the session bus) and exits.
class SingleInstance : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.solarizeddev.kyu.Panel")
public:
    using QObject::QObject;

    // True if this process is the panel; false if another one was asked to
    // come forward and this process should exit.
    bool claim();

public Q_SLOTS:
    // token: a Wayland activation token (may be empty).
    Q_SCRIPTABLE void Activate(const QString &token);

Q_SIGNALS:
    void activated(const QString &token);
};
