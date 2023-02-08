#pragma once

#include "dbuspropertycache.h"
#include <QMutex>
#include <QThread>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDBusPendingCallWatcher>

namespace DBusWrapper {

class PropertyCacheBackend : public QObject
{
    Q_OBJECT

public:
    PropertyCacheBackend(const Target& target);
    ~PropertyCacheBackend();

    static QSharedPointer<PropertyCacheBackend> instance(const Target& target);

    const Target m_target;
    QMutex m_dataMutex;
    QVariantMap m_properties;
    QDBusError m_error;
    bool m_available = false;

    static bool test_backendsEmpty();
    static void test_clearCache();

signals:
    void reset(const QVariantMap& values, QDBusError error = QDBusError());
    void changeProperties(const QVariantMap& values);

private slots:
    void serviceOwnerChanged(const QString& service, const QString& oldOwner, const QString& newOwner);
    void loadReply(QDBusPendingCallWatcher* w);
    void propertiesChanged(const QString& interface, QVariantMap values);

private:
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    QDBusPendingCallWatcher* m_pendingLoad = nullptr;
    QElapsedTimer m_loadTimer;

    Target propertiesTarget() const;
    void load();
    void doReset(const QVariantMap& properties, QDBusError error = QDBusError());
};

class PropertyCacheThreadData : public QObject
{
    Q_OBJECT

public:
    explicit PropertyCacheThreadData(const Target& target);
    ~PropertyCacheThreadData();

    const Target m_target;
    QVariantMap m_properties;
    QDBusError m_error;
    bool m_available = false;

    static QSharedPointer<PropertyCacheThreadData> localInstance(const Target& target);

signals:
    void availableChanged(bool available);
    void errorChanged(const QDBusError& error);
    void ready();
    void lost();

    void propertyChanged(const QString& property, const QVariant& value);
    void propertiesReset(const QVariantMap& properties);

private:
    QSharedPointer<PropertyCacheBackend> m_backend;

    void reset(const QVariantMap& values, const QDBusError& error);
    void changeProperties(const QVariantMap& values);
};

class PropertyCachePrivate
{
public:
    PropertyCachePrivate(PropertyCache* q, const Target& target);
    ~PropertyCachePrivate();

    PropertyCache* q;
    QSharedPointer<PropertyCacheThreadData> data;
    bool initialized = false;

    void initialize();
};

} // namespace DBusWrapper
