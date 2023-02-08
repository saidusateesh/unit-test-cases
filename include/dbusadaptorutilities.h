#pragma once

#include <QString>
#include <QVariant>
#include <QDBusConnection>

namespace DBusWrapper {

void emitPropertiesChanged(const QString& path, const QString& interface, const QString& property, const QVariant& value);
void emitPropertiesChanged(const QString& path, const QString& interface, const QVariantMap& changed_properties);

void emitPropertiesChanged(const QDBusConnection& bus, const QString& path, const QString& interface, const QString& property, const QVariant& value);
void emitPropertiesChanged(const QDBusConnection& bus, const QString& path, const QString& interface, const QVariantMap& changed_properties);

} // namepsace DBusWrapper
