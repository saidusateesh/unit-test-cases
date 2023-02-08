#include "dbusadaptorutilities.h"

#include <QDBusConnection>
#include <QDBusMessage>

#include "dbusutilities.h"

namespace DBusWrapper {

void emitPropertiesChanged(const QString &path, const QString &interface, const QString &property, const QVariant &value)
{
    emitPropertiesChanged(QDBusConnection::sessionBus(), path, interface, QVariantMap{{property, value}});
}

void emitPropertiesChanged(const QDBusConnection& bus, const QString& path, const QString& interface, const QString& property, const QVariant& value)
{
    emitPropertiesChanged(bus, path, interface, QVariantMap{{property, value}});
}

void emitPropertiesChanged(const QString &path, const QString &interface, const QVariantMap &changed_properties)
{
    emitPropertiesChanged(QDBusConnection::sessionBus(), path, interface, changed_properties);
}

void emitPropertiesChanged(const QDBusConnection& bus, const QString& path, const QString& interface, const QVariantMap& changed_properties)
{
    QDBusMessage signal = QDBusMessage::createSignal(path, k_property_interface, k_properties_changed_signal_name);
    signal << interface;
    signal << changed_properties;
    signal << QStringList();
    bus.send(signal);
}

} // namespace DBusWrapper
