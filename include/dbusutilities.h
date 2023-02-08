#pragma once

#include <QString>
#include <QVariant>
#include <QtDBus/QDBusVariant>

namespace DBusWrapper {

extern const QString k_property_interface;
extern const QString k_properties_changed_signal_name;

/*!
  Converts \a{value} to a QVariant for use as a DBus argument.

  This is equivalent to \l{QVariant::fromValue}, except that:
  \list
    \li \l{QVariant} values are wrapped with \l{QDBusVariant}
    \li \l{QDBusVariant} will not be double-wrapped
  \endlist

  This allows QVariant to be used normally when calling DBus APIs that expect a variant type.
 */
template<typename T> inline QVariant toDBusArgVariant(T value)
{
    return QVariant::fromValue(value);
}
template<> inline QVariant toDBusArgVariant<QVariant>(QVariant value)
{
    if (value.userType() == qMetaTypeId<QDBusVariant>())
        return value;
    return QVariant::fromValue(QDBusVariant(value));
}

} // namespace DBusWrapper
