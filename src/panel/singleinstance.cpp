#include "singleinstance.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace
{
const QString Service = QStringLiteral("io.github.solarizeddev.kyu.Panel");
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
    bus.call(QDBusMessage::createMethodCall(Service, QStringLiteral("/"), Service, QStringLiteral("Activate")));
    return false;
}

void SingleInstance::Activate()
{
    Q_EMIT activated();
}
