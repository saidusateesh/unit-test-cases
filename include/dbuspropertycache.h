#pragma once

#include <QObject>
#include <QVariant>
#include <QtDBus/QDBusConnection>
#include "dbustarget.h"

namespace DBusWrapper {

class PropertyCachePrivate;
class PropertyCache : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isAvailable READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(QDBusError error READ error NOTIFY errorChanged)

public:
    PropertyCache(const Target& target, QObject* parent = nullptr);
    PropertyCache(const QString& service, const QString& path, const QString& interface, QObject *parent = nullptr);
    PropertyCache(const QDBusConnection& bus, const QString& service, const QString& path, const QString& interface, QObject *parent = nullptr);
    virtual ~PropertyCache();

    QDBusConnection bus() const;
    const Target& target() const;
    bool isAvailable() const;
    QDBusError error() const;

    bool initialize();

    bool contains(const QString& property) const;
    QVariant get(const QString& property) const;
    template<typename T> T get(const QString& property) const
    {
        return get(property).template value<T>();
    }
    QVariantMap getAll() const;

    void set(const QString& property, const QVariant& value);

    void moveToThread(QThread*) = delete;

signals:
    void availableChanged(bool available);
    void errorChanged(const QDBusError& error);
    void ready();
    void lost();

    void propertyChanged(const QString& property, const QVariant& value);
    void propertiesReset(const QVariantMap& properties);

protected:
    virtual bool event(QEvent* e) override;

private:
    PropertyCachePrivate* d;
};



} // namepsace DBusWrapper
