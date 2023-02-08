#pragma once

#include "dbusutilities.h"
#include <QString>
#include <QHash>
#include <QDebug>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace DBusWrapper {

/*!
   \brief Represents the target of a DBus message.

   Target represents the tuple of (bus, service, path, interface) used by DBus messages.

   Target is copyable, movable, comparable, can be used as the key of \l{QHash}, and can be printed directly to \l{QDebug}.
 */
class Target
{
public:
    /*!
      \brief Construct a target with an explicit \a{bus}, \a{service}, \a{path}, and \a{interface}.
     */
    Target(const QDBusConnection& bus, const QString& service, const QString& path, const QString& interface)
        : m_bus(bus), m_service(service), m_path(path), m_interface(interface)
    {
    }
    /*!
      \brief Construct a target on the default session bus for \a{service}, \a{path}, and \a{interface}.
     */
    Target(const QString& service, const QString& path, const QString& interface)
        : Target(QDBusConnection::sessionBus(), service, path, interface)
    {
    }
    /*!
      \brief Construct an invalid target.
     */
    Target()
        : m_bus(QString())
    {
    }

    /*!
      \brief Returns true if the service, path, and interface are non-empty.
     */
    bool isValid() const { return !m_service.isEmpty() && !m_path.isEmpty() && !m_interface.isEmpty(); }
    /*!
      \brief Returns the \l{QDBusConnection}.
     */
    QDBusConnection bus() const { return m_bus; }
    /*!
      \brief Returns the service name.
     */
    QString service() const { return m_service; }
    /*!
      \brief Returns the object path.
     */
    QString path() const { return m_path; }
    /*!
      \brief Returns the interface name.
     */
    QString interface() const { return m_interface; }

    /*!
      \brief Returns a new target for \a{path} with the same bus, service, and interface.
     */
    Target withPath(const QString& path) const { return Target(m_bus, m_service, path, m_interface); }
    /*!
      \brief Returns a new target for \a{interface} with the same bus, service, and path.
     */
    Target withInterface(const QString& interface) const { return Target(m_bus, m_service, m_path, interface); }
    /*!
      \brief Returns a new target for \a{path} and \a{interface} with the same bus and service.
     */
    Target with(const QString& path, const QString& interface) const { return Target(m_bus, m_service, path, interface);}

    /*!
      \brief Creates a QDBusMessage for calling \a{method} on this target.

      Additional arguments are passed as arguments to the DBus method. For the call to succeed, argument types must match
      the DBus method definition.

      \note
      QVariant arguments are automatically wrapped in \l{QDBusVariant} and will be sent as the DBus 'variant' type.
     */
    template<typename... Args> QDBusMessage createMethodCall(const QString& method, Args... args)
    {
        auto msg = QDBusMessage::createMethodCall(m_service, m_path, m_interface, method);
        if (sizeof...(args) > 0)
            msg.setArguments(QVariantList{toDBusArgVariant(args)...});
        return msg;
    }

    Target(const Target& other) = default;
    Target(Target&& other) = default;
    Target& operator=(const Target& other) = default;
    bool operator==(const Target& other) const
    {
        return m_bus.name() == other.m_bus.name() &&
                m_service   == other.m_service &&
                m_path      == other.m_path &&
                m_interface == other.m_interface;
    }
    bool operator!=(const Target& other) const
    {
        return !(*this == other);
    }

    friend uint qHash(const Target& target, uint seed = 0) noexcept
    {
        QtPrivate::QHashCombine hash;
        seed = hash(seed, target.m_bus.name());
        seed = hash(seed, target.m_service);
        seed = hash(seed, target.m_path);
        seed = hash(seed, target.m_interface);
        return seed;
    }

    friend QDebug operator<<(QDebug debug, const Target& target)
    {
        QDebugStateSaver saver(debug);
        if (target.isValid()) {
            QString busName = target.m_bus.name();
            if (busName == "qt_default_session_bus")
                busName = "SessionBus";
            else if (busName == "qt_default_system_bus")
                busName = "SystemBus";
            debug.nospace().noquote() << "DBus(" << busName << ", " << target.m_service
                    << ", " << target.m_path << ", " << target.m_interface << ")";
        } else {
            debug << "DBus(invalid)";
        }
        return debug;
    }

private:
    QDBusConnection m_bus;
    QString m_service, m_path, m_interface;
};

} // namespace DBusWrapper

Q_DECLARE_METATYPE(DBusWrapper::Target)
