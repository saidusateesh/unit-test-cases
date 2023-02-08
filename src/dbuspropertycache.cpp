#include "dbuspropertycache_p.h"
#include "dbusutilities.h"
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QCoreApplication>
#include <QThread>
#include <QThreadStorage>
#include <QMutex>
#include <QTimer>

/*!
  \class DBusWrapper::PropertyCache
  \brief Asynchronous API for accessing DBus properties

  PropertyCache is a fully asynchronous API for accessing and monitoring DBus properties. Features include:
  \list
    \li Automatically loads/resets properties when the service (dis)connects
    \li Updates properties from DBus PropertiesChanged signals
    \li Shares data efficiently between PropertyCache instances within the same process
    \li Does not block on DBus under any circumstances
    \li Reliable API and race-free behavior
  \endlist

  \section1 Basic Usage
  \section2 Initialization

  PropertyCache loads properties automatically. Because data is loaded asynchronously, a newly constructed PropertyCache
  is \e{always} initially empty and unavailable. Once the thread returns to the event loop and data is available, the
  cache will initialize and emit signals.

  \section2 Availability

  The \l{isAvailable()} method indicates whether properties are available. It can be false for several reasons:
  \list
    \li The PropertyCache instance was just created
    \li The service isn't running, has exited, or doesn't exist
    \li The service reports that the path or interface doesn't exist
    \li A miscellaneous DBus error occurred
  \endlist

  If the service isn't running, PropertyCache will monitor DBus and automatically load properties when the service
  starts. Likewise, if the service exits (even unexpectedly) the cache will become unavailable and clear all properties.

  If \l{isAvailable()} returns true, \e{all} properties have values as returned by the service. Likewise, if it returns
  false, no property will have a value. The cache is \e{never} in a partially-initialized state, even while emitting
  signals.

  \section2 Getting properties
  The \l{get} method returns the cached value of a property. If the property doesn't exist or data isn't
  available yet, it returns an invalid QVariant instead. You can use this to provide a default value for
  uninitialized data:

  \code
    int Brightness::brightness() const
    {
        QVariant value = m_properties->get("Brightness");
        if (!value.isValid())
            return -1;
        return value.toInt();
    }
  \endcode

  Alternatively, the \l{get<T>} method converts directly and returns a default-constructed value if the
  property is not available:

  \code
    bool Network::isOnline() const
    {
        return m_properties->get<bool>("Online");
    }
  \endcode

  \section2 Signals

  When data becomes available, PropertyCache will:
  \list 1
    \li Set values for all properties internally and update \l{isAvailable}
    \li Emit \l{availableChanged}{availableChanged(true)}
    \li Emit \l{errorChanged} if appropriate
    \li Emit \l{propertiesReset} with all properties
    \li Emit \l{propertyChanged} for every property
    \li Emit \l{ready}
  \endlist

  This order is very important. For example, \l{get()} will always return full and consistent data, even in the middle
  of a series of signals. Also, \l{propertyChanged} will be emitted for every property before emitting \l{ready}.

  When data becomes unavailable (e.g. because the service exited), PropertyCache will:
  \list 1
    \li Clear all properties internally and update \l{isAvailable} and \l{error}
    \li Emit \l{availableChanged}{availableChanged(false)}
    \li Emit \l{errorChanged}
    \li Emit \l{propertiesReset} with an empty map, if any properties had values
    \li Emit \l{propertyChanged} for every property that had a value
    \li Emit \l{lost}
  \endlist

  Of course, \l{propertyChanged} is emitted whenever the property's value changes.

  \section2 Setting properties
  If a DBus property is writable, you can use \l{set} to attempt to change the value. However, this has important
  caveats:

  \list
    \li \l{set} is asynchronous and \e{does not immediately update the value}.
    \li \l{set} can fail and does not report any errors.
    \li The service can choose to do nothing or to use a different value.
  \endlist

  In other words, \l{set} \e{requests} that the service change the value, but it does not actually change unless/until
  the service emits the PropertiesChanged DBus signal.

  \note
  We plan to allow \l{set} to return an asynchronous result in the future.

  \section1 Advanced Usage
  \section2 Multi-threaded applications

  An instance of PropertyCache has \l{thread affinity} to the thread where it was created and \b{must not} be accessed
  from any other thread, not even with a mutex. It is not possible to move a PropertyCache instance to a different
  thread.

  PropertyCache also requires the thread to run an event loop. This means it's generally not possible to use on threads
  that run a single function (e.g. with QThread::create or QtConcurrent) unless that function also processes events.

  \note
  These restrictions are common for many QObject-derived types. For example, using QTimer or queued signal connections
  also requires an event loop. The Qt documentation on \l{Threads and QObjects} explains this in more detail.

  \section3 Singletons
  Historically, a common pattern has been to create a singleton type to provide information from a DBus interface, such
  as in libvehicle. \e{This is fundamentally unsafe} in a multi-threaded application: even if each individual function
  is thread-safe, the data can be change between function calls, so there is no way to provide consistency.

  The sections below describe an alternative approach, where data is efficiently shared between multiple instances of
  a type.

  \section2 Shared data
  Internally, instances of PropertyCache with the same target (bus, service, path, and interface) will share data to
  avoid redundant DBus activity. This means that constructing a PropertyCache does not require any DBus calls if that
  data exists anywhere else in the process.

  However, a newly constructed PropertyCache is \e{always} uninitialized, even if the data could be available
  immediately. This allows you to connect signals before data is initialized and have consistent behavior in all cases.
  PropertyCache will initialize using the shared data and emit signals after the thread returns to the event loop.

  \section3 Immediate initialization
  Optionally, you can call \l{initialize()} after connecting signals to do this immediately instead.

  \e{If no DBus calls are pending}, \l{initialize} emits the same signals immediately and returns true. Note that this
  does not mean \l{isAvailable} is true if the service is offline or previously returned an error.

  If DBus calls are required (i.e., there was no existing instance of this cache), \l{initialize} does nothing and
  returns false.

  An idiomatic way to use this feature would be:

  \code
    NetworkStatus::NetworkStatus(QObject* parent)
      : m_properties(new DBusWrapper::PropertyCache("org.example", "/org/example", "org.example", this))
    {
        connect(m_properties, &DBusWrapper::PropertyCache::propertyChanged, this, &NetworkStatus::onPropertyChanged);
        connect(m_properties, &DBusWrapper::PropertyCache::availableChanged, this, &NetworkStatus::dataAvailableChanged);
        m_properties->initialize();
    }
  \endcode

  \section2 Consistency
  PropertyCache guarantees a consistent view of data (as provided by the service) at all times. Specifically:
  \list
    \li If \l{isAvailable()} is true, all properties provided by the service have values.
    \li If \l{isAvailable()} is false, no property has a value.
    \li The \l{propertyChanged()} signal is emitted after a property's value changes for any reason.
    \li PropertyCache will not change in any way until control returns to the event loop (except for calls to \l{initialize()}).
  \endlist

  \section3 Atomic signals
  The DBus PropertiesChanged signal can change multiple properties simultaneously, and PropertyCache guarantees that
  these changes are atomic. In other words, all changes from a single DBus message apply simultaneously before
  PropertyCache emits any signal. If multiple DBus messages are queued, each is applied separately.

  \section3 Per-thread consistency
  Initialized instances of PropertyCache for the same target \e{on the same thread} are guaranteed to return the same
  values at all times, and signals emitted by these instances are interleaved. In other words, if two properties change,
  all instances on the thread will apply both changes, then each will emit propertyChanged for the first property, and
  finally each will emit propertyChanged for the second property.
 */

namespace DBusWrapper {

Q_LOGGING_CATEGORY(logPropertyCache, "dbuswrapper.propertycache", QtWarningMsg)
Q_LOGGING_CATEGORY(logPropertyCacheData, "dbuswrapper.propertycache.data", QtWarningMsg)
Q_LOGGING_CATEGORY(logCacheInternal, "dbuswrapper.propertycache.internal", QtWarningMsg)

// PropertyCache is the end-user API. Each of these has a PropertyCachePrivate, which exists to keep the implementation
// details out of the A[BP]I.
//
// PropertyCachePrivate holds a reference to the PropertyCacheThreadData. This is shared with other instances that have
// the same target _and_ thread affinity. That is managed through the thread-local `cacheThreadData` map.
//
// PropertyCacheThreadData holds a thread's view of the data for a target and is only updated on that thread. This
// serves a few goals:
//   - it's safe to use without locking
//   - data is consistent and won't change between calls
//   - data is consistent between multiple PropertyCache instances on the same thread, even during signals
//
// Finally, there's an instance of PropertyCacheBackend for each target, shared by all threads. The backend objects
// live on the dedicated `backendThread`, where they manage all DBus activity and emit signals to the ThreadData
// instances.
//
// Each PropertyCacheThreadData holds a QSharedPointer reference to the backend. When there are no remaining references,
// the backend is _not_ deleted immediately. Instead, ownership is transferred to the 'unusedCacheBackends' list, which
// keeps a number of recently-unused backends alive in case they're needed again. This helps avoid expensive DBus calls
// in certain situations.
//
// When the unusedCacheBackends list is full, the oldest item is scheduled for deletion from the backendThread.

// Holds a per-thread map of property cache data. This is safe to access from the associated thread without locking.
static QThreadStorage<QHash<Target, QWeakPointer<PropertyCacheThreadData>>> cacheThreadData;

static std::unique_ptr<QThread> backendThread;
static QMutex backendsMutex;
// Holds weak references to all referenced PropertyCacheBackend instances. Must hold backendsMutex to access.
static QHash<DBusWrapper::Target, QWeakPointer<PropertyCacheBackend>> cacheBackends;
// Holds all unreferenced PropertyCacheBackend instances. Must hold backendsMutex to access.
static constexpr int unusedCacheCapacity = 5;
static QVarLengthArray<DBusWrapper::PropertyCacheBackend*, unusedCacheCapacity> unusedCacheBackends;

PropertyCache::PropertyCache(const Target& target, QObject *parent)
    : QObject(parent)
    , d(new PropertyCachePrivate(this, target))
{
}

PropertyCache::PropertyCache(const QString& service, const QString& path, const QString& interface, QObject *parent)
    : PropertyCache(Target(service, path, interface), parent)
{
}

PropertyCache::PropertyCache(const QDBusConnection& bus, const QString& service, const QString& path, const QString& interface, QObject *parent)
    : PropertyCache(Target(bus, service, path, interface), parent)
{
}

PropertyCache::~PropertyCache()
{
    delete d;
}

const Target& PropertyCache::target() const
{
    return d->data->m_target;
}

QDBusConnection PropertyCache::bus() const
{
    return d->data->m_target.bus();
}

bool PropertyCache::isAvailable() const
{
    return d->initialized && d->data->m_available;
}

QDBusError PropertyCache::error() const
{
    if (d->initialized)
        return d->data->m_error;
    else
        return {};
}

bool PropertyCache::initialize()
{
    d->initialize();
    return (d->data->m_available || d->data->m_error.isValid());
}

QVariant PropertyCache::get(const QString& property) const
{
    if (!d->initialized)
        return {};
    return d->data->m_properties.value(property);
}

bool PropertyCache::contains(const QString& property) const
{
    if (!d->initialized)
        return false;
    return d->data->m_properties.contains(property);
}

QVariantMap PropertyCache::getAll() const
{
    if (!d->initialized)
        return {};
    return d->data->m_properties;
}

void PropertyCache::set(const QString& property, const QVariant& value)
{
    const auto& target = d->data->m_target;
    auto msg = target.withInterface(k_property_interface).createMethodCall("Set",
                                                      target.interface(),
                                                      property,
                                                      value);
    auto reply = target.bus().asyncCall(msg);
    auto pendingSet = new QDBusPendingCallWatcher(reply, this);
    connect(pendingSet, &QDBusPendingCallWatcher::finished, this, [this, property](QDBusPendingCallWatcher *w){
        w->deleteLater();

        QDBusPendingReply<> reply = *w;
        if (reply.isError()) {
            qCWarning(logPropertyCache) << "failed to set property" << property << "for" << d->data->m_target << "with error" << reply.error();
        }
    });
}

bool PropertyCache::event(QEvent* ev)
{
    if (ev->type() == QEvent::ThreadChange) {
        qCCritical(logPropertyCache) << "BUG: thread changed for" << this << "on" << d->data->m_target
            << "- PropertyCache does not support moveToThread";
    }
    return QObject::event(ev);
}

PropertyCachePrivate::PropertyCachePrivate(PropertyCache* q, const Target& target)
    : q(q), data(PropertyCacheThreadData::localInstance(target))
{
    qCDebug(logCacheInternal) << "created PropertyCache for" << target << "on" << q->thread();
    // If data is _not_ available, initializing just connects the signals, so it can happen immediately.
    //
    // If there's already data, pretend it doesn't exist yet and initialize on the next loop. This gives
    // the caller time to connect their own signals and get consistent behavior.
    if (!data->m_available && !data->m_error.isValid()) {
        initialize();
    } else {
        QMetaObject::invokeMethod(q, [this]() { initialize(); }, Qt::QueuedConnection);
    }
}

PropertyCachePrivate::~PropertyCachePrivate()
{
    qCDebug(logCacheInternal) << "destroyed PropertyCache for" << data->m_target;
}

void PropertyCachePrivate::initialize()
{
    if (initialized)
        return;

    QObject::connect(data.get(), &PropertyCacheThreadData::availableChanged, q, &PropertyCache::availableChanged);
    QObject::connect(data.get(), &PropertyCacheThreadData::errorChanged, q, &PropertyCache::errorChanged);
    QObject::connect(data.get(), &PropertyCacheThreadData::ready, q, &PropertyCache::ready);
    QObject::connect(data.get(), &PropertyCacheThreadData::lost, q, &PropertyCache::lost);
    QObject::connect(data.get(), &PropertyCacheThreadData::propertyChanged, q, &PropertyCache::propertyChanged);
    QObject::connect(data.get(), &PropertyCacheThreadData::propertiesReset, q, &PropertyCache::propertiesReset);
    initialized = true;
    if (data->m_error.isValid())
        emit q->errorChanged(data->m_error);
    if (!data->m_available)
        return;

    // Emit signals in the same order as PropertyCacheData::resetProperties
    emit q->availableChanged(true);
    emit q->propertiesReset(data->m_properties);
    for (auto it = data->m_properties.constBegin(); it != data->m_properties.constEnd(); it++) {
        emit q->propertyChanged(it.key(), it.value());
    }
    emit q->ready();
}

QSharedPointer<PropertyCacheThreadData> PropertyCacheThreadData::localInstance(const Target& target)
{
    auto& instances = cacheThreadData.localData();
    auto it = instances.find(target);
    if (it != instances.end()) {
        if (auto ref = it->toStrongRef())
            return ref;
    }
    auto ref = QSharedPointer<PropertyCacheThreadData>::create(target);
    instances.insert(target, ref.toWeakRef());
    return ref;
}

PropertyCacheThreadData::PropertyCacheThreadData(const Target& target)
    : m_backend(PropertyCacheBackend::instance(target)), m_target(target)
{
    qCDebug(logCacheInternal) << "created" << this << "for" << m_target << "on" << thread();
    QMutexLocker lock(&m_backend->m_dataMutex);
    connect(m_backend.get(), &PropertyCacheBackend::reset, this, &PropertyCacheThreadData::reset);
    connect(m_backend.get(), &PropertyCacheBackend::changeProperties, this, &PropertyCacheThreadData::changeProperties);
    m_properties = m_backend->m_properties;
    m_error = m_backend->m_error;
    m_available = m_backend->m_available;
    lock.unlock();
}

PropertyCacheThreadData::~PropertyCacheThreadData()
{
    qCDebug(logCacheInternal) << "destroyed" << this << "for" << m_target << "on" << thread();
    auto weakRef = cacheThreadData.localData().take(m_target);
    Q_ASSERT(weakRef.isNull());
    Q_UNUSED(weakRef);
}

void PropertyCacheThreadData::reset(const QVariantMap& values, const QDBusError& error)
{
    bool available = !error.isValid();
    Q_ASSERT(available || values.isEmpty());

    // The order here is very specific:
    //   1. Update state internally
    //   2. Emit availableChanged and errorChanged if appropriate
    //   3. Emit propertiesReset and lost
    //   4. Emit propertyChanged as needed
    //   5. Emit ready
    bool wasAvailable = m_available;
    QVariantMap before = m_properties;
    bool errorChange = (m_error.type() != error.type());

    m_available = available;
    m_error = error;
    m_properties = values;

    if (wasAvailable != m_available)
        emit availableChanged(m_available);
    if (errorChange)
        emit errorChanged(error);
    if (!values.isEmpty() || !before.isEmpty())
        emit propertiesReset(m_properties);

    for (auto it = m_properties.constBegin(); it != m_properties.constEnd(); it++) {
        auto beforeIt = before.constFind(it.key());
        if (beforeIt == before.constEnd() || beforeIt.value() != it.value()) {
            emit propertyChanged(it.key(), it.value());
        }
    }
    for (auto it = before.constBegin(); it != before.constEnd(); it++) {
        if (!m_properties.contains(it.key())) {
            emit propertyChanged(it.key(), QVariant());
        }
    }

    if (wasAvailable && !m_available)
        emit lost();
    if (!wasAvailable && m_available)
        emit ready();
}

void PropertyCacheThreadData::changeProperties(const QVariantMap& values)
{
    // Update all values before sending any signals
    for (auto it = values.constBegin(); it != values.constEnd(); it++)
        m_properties[it.key()] = it.value();
    for (auto it = values.constBegin(); it != values.constEnd(); it++)
        emit propertyChanged(it.key(), it.value());
}

static void backendReleased(DBusWrapper::PropertyCacheBackend* backend)
{
    QMutexLocker l(&backendsMutex);
    qCDebug(logCacheInternal) << "released" << backend << "for" << backend->m_target << "to unreferenced cache";
    // remove the dead weak pointer
    auto it = cacheBackends.find(backend->m_target);
    if (it != cacheBackends.end() && it->isNull())
        cacheBackends.erase(it);
    // transfer ownership of the instance to the unused list
    while (unusedCacheBackends.size() >= unusedCacheCapacity) {
        unusedCacheBackends.last()->deleteLater();
        unusedCacheBackends.removeLast();
    }
    unusedCacheBackends.prepend(backend);
}

QSharedPointer<PropertyCacheBackend> PropertyCacheBackend::instance(const Target& target)
{
    QMutexLocker l(&backendsMutex);
    // Search the existing referenced backends
    auto it = cacheBackends.find(target);
    if (it != cacheBackends.end()) {
        if (auto ref = it->toStrongRef())
            return ref;
    }

    // Search the cache of unreferenced backends and restore if found
    for (int i = 0; i < unusedCacheBackends.size(); i++) {
        auto cachedBackend = unusedCacheBackends[i];
        if (cachedBackend->m_target == target) {
            unusedCacheBackends.remove(i);
            qCDebug(logCacheInternal) << "restored backend" << cachedBackend << "from unused cache for" << target;
            QSharedPointer<PropertyCacheBackend> ref(cachedBackend, &backendReleased);
            cacheBackends.insert(target, ref.toWeakRef());
            return ref;
        }
    }

    // Create a new backend
    QSharedPointer<PropertyCacheBackend> ref(new PropertyCacheBackend(target), &backendReleased);
    cacheBackends.insert(target, ref.toWeakRef());
    return ref;
}

static void cleanupBackendThread()
{
    qCDebug(logCacheInternal) << "cleaning up DBusWrapper backend thread";
    {
        QMutexLocker lock(&backendsMutex);
        for (auto b : unusedCacheBackends)
            b->deleteLater();
        unusedCacheBackends.clear();
    }
    backendThread->quit();
    backendThread->wait(5000);
    backendThread.reset();
}

PropertyCacheBackend::PropertyCacheBackend(const Target& target)
    : m_target(target)
{
    // backend lock is held
    if (!backendThread) {
        backendThread = std::make_unique<QThread>();
        backendThread->setObjectName("DBusWrapper");
        backendThread->moveToThread(qApp->thread());
        qAddPostRoutine(cleanupBackendThread);
        backendThread->start();
    }

    qCDebug(logCacheInternal) << "created" << this << "for" << target;

    moveToThread(backendThread.get());
    bool ok = QMetaObject::invokeMethod(this, &PropertyCacheBackend::load, Qt::QueuedConnection);
    Q_ASSERT(ok);
    Q_UNUSED(ok);
}

PropertyCacheBackend::~PropertyCacheBackend()
{
    // NOTE: backend lock may or may not be held; don't use it or rely on it here
    qCDebug(logCacheInternal) << "destroyed" << this << "for" << m_target;
    m_target.bus().disconnect(m_target.service(), m_target.path(), k_property_interface, k_properties_changed_signal_name,
                              {m_target.interface()}, QString(), this, SLOT(propertiesChanged(QString,QVariantMap)));
}

bool PropertyCacheBackend::test_backendsEmpty()
{
    QMutexLocker lock(&backendsMutex);
    return cacheBackends.isEmpty();
}

void PropertyCacheBackend::test_clearCache()
{
    QMutexLocker lock(&backendsMutex);
    for (auto b : unusedCacheBackends)
        b->deleteLater();
    unusedCacheBackends.clear();
}

Target PropertyCacheBackend::propertiesTarget() const
{
    return m_target.withInterface(k_property_interface);
}

void PropertyCacheBackend::load()
{
    if (m_pendingLoad)
        return;
    if (logPropertyCache().isDebugEnabled())
        m_loadTimer.start();

    if (!m_watcher) {
        m_watcher = std::make_unique<QDBusServiceWatcher>(m_target.service(), m_target.bus(), QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this, &PropertyCacheBackend::serviceOwnerChanged);
        m_target.bus().connect(m_target.service(), m_target.path(), k_property_interface, k_properties_changed_signal_name, {m_target.interface()}, QString(),
                    this, SLOT(propertiesChanged(QString,QVariantMap)));
    }

    auto msg = propertiesTarget().createMethodCall("GetAll", m_target.interface());
    auto reply = m_target.bus().asyncCall(msg);
    m_pendingLoad = new QDBusPendingCallWatcher(reply, this);
    connect(m_pendingLoad, &QDBusPendingCallWatcher::finished, this, &PropertyCacheBackend::loadReply);
}

void PropertyCacheBackend::loadReply(QDBusPendingCallWatcher *w)
{
    w->deleteLater();
    if (w != m_pendingLoad)
        return;
    m_pendingLoad = nullptr;
    QDBusPendingReply<QVariantMap> reply = *w;

    if (reply.isError()) {
        if (reply.error().type() == QDBusError::ServiceUnknown) {
            qCInfo(logPropertyCache) << "service" << m_target.service() << "is unavailable, waiting to load properties from" << m_target;
        } else {
            qCWarning(logPropertyCache) << "loading properties from" << m_target << "failed:" << reply.error();
        }

        doReset(QVariantMap(), reply.error());
    } else {
        qCDebug(logPropertyCache) << "received properties from" << m_target << "in" << m_loadTimer.elapsed() << "ms";
        doReset(reply.value());
    }
}

void PropertyCacheBackend::serviceOwnerChanged(const QString& service, const QString& oldOwner, const QString& newOwner)
{
    Q_UNUSED(service)
    Q_UNUSED(oldOwner)

    if (m_pendingLoad) {
        qCDebug(logPropertyCache) << "service owner changed, canceling pending property load from" << m_target;
        m_pendingLoad->deleteLater();
        m_pendingLoad = nullptr;
    }

    if (newOwner.isEmpty()) {
        qCInfo(logPropertyCache) << "service disconnected, resetting properties for" << m_target;
        doReset(QVariantMap(), QDBusError(QDBusError::ServiceUnknown, "DBus service disconnected"));
    } else {
        qCInfo(logPropertyCache) << "service is now available, loading properties from" << m_target;
        // Delay the call slightly to give the service a chance to finish starting up and make it more likely that
        // we'll get a useful result the first time. If a PropertiesChanged signal arrives first, that will trigger
        // an immediate load.
        QTimer::singleShot(50, this, &PropertyCacheBackend::load);
    }
}

void PropertyCacheBackend::doReset(const QVariantMap& properties, QDBusError error)
{
    QMutexLocker lock(&m_dataMutex);
    if (logPropertyCacheData().isDebugEnabled() && (!m_properties.isEmpty() || !properties.isEmpty())) {
        qCDebug(logPropertyCacheData) << "reset" << m_target << m_properties.keys();
        for (auto it = properties.constBegin(); it != properties.constEnd(); it++)
            qCDebug(logPropertyCacheData) << m_target << it.key() << "=" << it.value();
    }
    m_properties = properties;
    m_error = error;
    m_available = !error.isValid();
    emit reset(properties, error);
}

void PropertyCacheBackend::propertiesChanged(const QString&, QVariantMap values)
{
    // Ignore changes while waiting for a reply to GetAll. Emitting any signals would break API
    // guarantees, and any values here will also be in the reply.
    if (m_pendingLoad) {
        qCDebug(logPropertyCache) << "ignored property change signal while loading properties from" << m_target;
        return;
    }
    QMutexLocker lock(&m_dataMutex);
    if (!m_available) {
        qCDebug(logPropertyCache) << "retrying load after receiving unexpected PropertiesChanged from" << m_target
                                  << "which was unavailable because" << m_error;
        lock.unlock();
        load();
        return;
    }

    for (auto it = values.begin(); it != values.end(); ) {
        qCDebug(logPropertyCacheData) << "change" << m_target << it.key() << "=" << it.value();
        auto cacheIt = m_properties.find(it.key());
        if (cacheIt == m_properties.end()) {
            m_properties.insert(it.key(), it.value());
            it++;
        } else if (cacheIt.value() != it.value()) {
            cacheIt.value() = it.value();
            it++;
        } else {
            it = values.erase(it);
        }
    }
    emit changeProperties(values);
}

} // namespace DBusWrapper
