#include "singleinstance.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace
{
const QString Service = QStringLiteral("io.github.solarizeddev.kyu.Card");
}

bool SingleInstance::claim()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return true; // no session bus: nothing to coordinate with
    }
    if (bus.registerService(Service)) {
        bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
        return true;
    }
    // Pass on the token this process was started with (from the tray click).
    auto call = QDBusMessage::createMethodCall(Service, QStringLiteral("/"), Service, QStringLiteral("Activate"));
    call.setArguments({qEnvironmentVariable("XDG_ACTIVATION_TOKEN")});
    if (bus.call(call).type() != QDBusMessage::ErrorMessage) {
        return false;
    }
    // The other card was just closing: this one takes over.
    if (bus.registerService(Service)) {
        bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
    }
    return true;
}

void SingleInstance::Activate(const QString &token)
{
    Q_EMIT activated(token);
}
