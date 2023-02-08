#include <QTest>
#include <QSignalSpy>
#include <QRegularExpression>
#include "../src/dbuspropertycache_p.h"
#include "dbusadaptorutilities.h"
#include "testbus.h"
#include "testservice.h"

static const QString testService = QStringLiteral("test.service");
static const QString testPath = QStringLiteral("/test/service");
static const QString testInterface = QStringLiteral("test.service");

class PropertyService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "test.service")

public:
    QDBusConnection m_bus;

    enum class InitMode
    {
        Normal,
        NoRegisterObject
    };

    PropertyService(const QDBusConnection& bus, InitMode mode = InitMode::Normal, QObject* parent = nullptr)
        : QObject(parent), m_bus(bus)
    {
        if (mode != InitMode::NoRegisterObject)
            registerObject();
        m_bus.registerService(testService);
    }
    ~PropertyService() {
        m_bus.unregisterService(testService);
        m_bus.unregisterObject(testPath);
    }

    void registerObject()
    {
        m_bus.registerObject(testPath, this, QDBusConnection::ExportAllProperties);
    }

    int strGetCount = 0;

    Q_PROPERTY(QString str READ getStr WRITE setStr)
    QString str = "hello";
    QString getStr()
    {
        strGetCount++;
        return str;
    }
    void setStr(const QString& value)
    {
        str = value;
        DBusWrapper::emitPropertiesChanged(m_bus, testPath, testInterface, "str", value);
    }

    Q_PROPERTY(QVariant variant MEMBER variant WRITE setVariant)
    QVariant variant;
    void setVariant(const QVariant& value)
    {
        variant = value;
        DBusWrapper::emitPropertiesChanged(m_bus, testPath, testInterface, "variant", value);
    }

    void setBoth(const QVariant& v, const QString& s)
    {
        variant = v;
        str = s;
        DBusWrapper::emitPropertiesChanged(m_bus, testPath, testInterface, {{"variant", v}, {"str", s}});
    }
};

class TestPropertyCache : public QObject
{
    Q_OBJECT

    std::unique_ptr<DBusTest::TestBus> dbus;

private slots:
    void init()
    {
        dbus = std::make_unique<DBusTest::TestBus>();
        QVERIFY(dbus->isValid());
    }

    void cleanup()
    {
        auto bus = std::move(dbus);
        DBusWrapper::PropertyCacheBackend::test_clearCache();
        QTRY_VERIFY(DBusWrapper::PropertyCacheBackend::test_backendsEmpty());
        QVERIFY(bus->waitForAllDisconnected());
    }

    void serviceAvailability()
    {
        DBusWrapper::PropertyCache cache(dbus->client(), testService, testPath, testInterface);
        QVERIFY(!cache.isAvailable());
        QVERIFY(!cache.error().isValid());

        QSignalSpy spyAvailableChanged(&cache, &DBusWrapper::PropertyCache::availableChanged);
        QSignalSpy spyReady(&cache, &DBusWrapper::PropertyCache::ready);
        QSignalSpy spyLost(&cache, &DBusWrapper::PropertyCache::lost);
        QSignalSpy spyPropertiesReset(&cache, &DBusWrapper::PropertyCache::propertiesReset);
        QSignalSpy spyError(&cache, &DBusWrapper::PropertyCache::errorChanged);

        // wait for initialization to fail with ServiceUnknown
        QVERIFY(spyError.wait());
        spyError.takeFirst();
        QCOMPARE(cache.error().type(), QDBusError::ServiceUnknown);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(spyPropertiesReset.count(), 0);
        QCOMPARE(spyLost.count(), 0);
        QCOMPARE(spyAvailableChanged.count(), 0);

        // Bring the service online, check that it initialized correctly, then take it down again
        {
            DBusTest::TestService<PropertyService> service(*dbus);
            QVERIFY(expectInitialization(&cache, InitializeNormally));
            // errorChanged to no error
            QCOMPARE(spyError.count(), 1);
            QVERIFY(!spyError.takeFirst()[0].value<QDBusError>().isValid());
            // expectInitialization checks the signals, so just reset these spies
            spyAvailableChanged.takeFirst();
            spyReady.takeFirst();
        }
        spyLost.wait();
        QCOMPARE(spyError.count(), 1);
        QCOMPARE(spyError.takeFirst()[0].value<QDBusError>().type(), QDBusError::ServiceUnknown);
        QCOMPARE(cache.error().type(), QDBusError::ServiceUnknown);
        QVERIFY(!cache.isAvailable());
        QCOMPARE(spyAvailableChanged.count(), 1);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(spyLost.count(), 1);

        // Bring the service online once more and make sure it re-initializes
        QVariantMap allProperties;
        {
            DBusTest::TestService<PropertyService> service(*dbus);
            QVERIFY(expectInitialization(&cache, InitializeNormally));

            allProperties = cache.getAll();
            QVERIFY(!allProperties.isEmpty());

            // Make sure propertyChanged is emitted to clear every existing property
            connect(&cache, &DBusWrapper::PropertyCache::propertyChanged, [&](auto property, auto value) {
                QVERIFY(!value.isValid());
                QVERIFY(!cache.contains(property));
                QVERIFY(allProperties.remove(property));
            });
            connect(&cache, &DBusWrapper::PropertyCache::propertiesReset, [&](auto properties) {
                QVERIFY(properties.isEmpty());
                QVERIFY(cache.getAll().isEmpty());
            });
        }
        spyLost.wait();
        QVERIFY(allProperties.isEmpty());
    }

    void initialization()
    {
        DBusTest::TestService<PropertyService> service(*dbus);
        DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
        QVERIFY(expectInitialization(&cache0, InitializeNormally));

        // Normal initialization (on the next loop)
        {
            DBusWrapper::PropertyCache cache1(dbus->client(), testService, testPath, testInterface);
            QVERIFY(!cache1.isAvailable());
            QVERIFY(cache1.getAll().isEmpty());
            QVERIFY(!cache1.contains("str"));
            QVERIFY(!cache1.get("str").isValid());
            QVERIFY(expectInitialization(&cache1, InitializeNormally));
        }

        // Immediate initialization
        {
            DBusWrapper::PropertyCache cache2(dbus->client(), testService, testPath, testInterface);
            QVERIFY(expectInitialization(&cache2, InitializeImmediately));
        }

        // Properties should have only been requested once
        service.sync([](auto s) { QCOMPARE(s->strGetCount, 1); });
    }

    void initializationWithError()
    {
        DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
        QSignalSpy spyError(&cache0, &DBusWrapper::PropertyCache::errorChanged);

        // wait for initialization to fail with ServiceUnknown
        QVERIFY(spyError.wait());
        spyError.takeFirst();
        QCOMPARE(cache0.error().type(), QDBusError::ServiceUnknown);

        // Create another PropertyCache and make sure it initializes to the error correctly
        {
            DBusWrapper::PropertyCache cache1(dbus->client(), testService, testPath, testInterface);
            QVERIFY(!cache1.error().isValid());
            QSignalSpy spyError1(&cache1, &DBusWrapper::PropertyCache::errorChanged);
            QVERIFY(cache1.initialize());
            QCOMPARE(spyError1.count(), 1);
            QCOMPARE(cache1.error().type(), cache0.error().type());
        }
    }

    void initializationMultiThread()
    {
        DBusTest::TestService<PropertyService> service(*dbus);
        DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
        QVERIFY(expectInitialization(&cache0, InitializeNormally));

        QScopedPointer<QThread> thread(QThread::create([this](const QDBusConnection& bus) {
            DBusWrapper::PropertyCache cache1(bus, testService, testPath, testInterface);
            // Should be able to initialize immediately using the shared backend
            QVERIFY(expectInitialization(&cache1, InitializeImmediately));
        }, dbus->client()));
        thread->start();
        thread->wait(5000);
        QVERIFY(thread->isFinished());

        // Properties should have only been requested once
        service.sync([](auto s) { QCOMPARE(s->strGetCount, 1); });
    }

    void initializationMultiThreadError()
    {
        DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
        QTRY_COMPARE(cache0.error().type(), QDBusError::ServiceUnknown);

        QScopedPointer<QThread> thread(QThread::create([this](const QDBusConnection& bus) {
            DBusWrapper::PropertyCache cache1(bus, testService, testPath, testInterface);
            QVERIFY(!cache1.isAvailable());
            QVERIFY(!cache1.error().isValid());
            // Should be able to initialize immediately and see the error
            QSignalSpy spyError(&cache1, &DBusWrapper::PropertyCache::errorChanged);
            QVERIFY(cache1.initialize());
            QCOMPARE(spyError.count(), 1);
            QCOMPARE(cache1.error().type(), QDBusError::ServiceUnknown);
        }, dbus->client()));
        thread->start();
        thread->wait(5000);
        QVERIFY(thread->isFinished());
    }

    void cachePersistence()
    {
        DBusTest::TestService<PropertyService> service(*dbus);

        // Create a cache object, initialize, and destroy it
        {
            DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
            QTRY_VERIFY(cache0.isAvailable());
        }

        // Create the same cache again. The backend should stay alive for some time even though
        // there are no explicit refs remaining, so it should be available immediately.
        {
            DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
            QVERIFY(expectInitialization(&cache0, InitializeImmediately));
        }

        // Create and destroy N other backend instances to fill up the cache. Must match unusedCacheCapacity.
        for (int i = 0; i < 5; i++) {
            DBusWrapper::PropertyCache cacheN(dbus->client(), testService, QString("/test/path/%1").arg(i), testInterface);
        }

        // Verify that the first instance is no longer possible to initialize immediately
        DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
        QVERIFY(!cache0.initialize());
    }

    void destroyQuickly()
    {
        DBusWrapper::PropertyCache cache(dbus->client(), testService, testPath, testInterface);
        // should not segfault
    }

    void propertyChanges()
    {
        DBusTest::TestService<PropertyService> service(*dbus);
        DBusWrapper::PropertyCache cache(dbus->client(), testService, testPath, testInterface);
        QTRY_VERIFY(cache.isAvailable());
        QCOMPARE(cache.get<QString>("str"), "hello");

        // emit three changes, one of them redundant. should receive two signals and have the correct state.
        service.invoke([](auto s) { s->setStr("one"); s->setStr("one"); s->setStr("two"); });
        QStringList expected{"one", "two"};
        connect(&cache, &DBusWrapper::PropertyCache::propertyChanged, [&](auto property, auto value) {
            QCOMPARE(property, "str");
            QVERIFY(!expected.isEmpty());
            QCOMPARE(value, expected.takeFirst());
            QCOMPARE(cache.get<QString>("str"), value);
        });
        QTRY_VERIFY(expected.isEmpty());
    }

    void propertyChangeSignalAtomic()
    {
        DBusTest::TestService<PropertyService> service(*dbus);
        DBusWrapper::PropertyCache cache(dbus->client(), testService, testPath, testInterface);
        QTRY_VERIFY(cache.isAvailable());

        // emit a single signal changing "variant" and "str"
        service.invoke([](auto s) { s->setBoth(999, "test"); });
        QList<std::pair<QString, QVariant>> expected{{"str", "test"}, {"variant", 999}};
        connect(&cache, &DBusWrapper::PropertyCache::propertyChanged, [&](auto property, auto value) {
            QVERIFY(!expected.isEmpty());
            auto expect = expected.takeFirst();
            QCOMPARE(expect.first, property);
            QCOMPARE(expect.second, value);
            // Signals apply atomically; both changes should be applied before the first signal.
            QCOMPARE(cache.get("variant"), 999);
            QCOMPARE(cache.get("str"), "test");
        });
        QTRY_VERIFY(expected.isEmpty());
    }

    void propertyChangeThreadAtomic()
    {
        DBusTest::TestService<PropertyService> service(*dbus);
        DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
        QTRY_VERIFY(cache0.isAvailable());
        DBusWrapper::PropertyCache cache1(dbus->client(), testService, testPath, testInterface);
        QVERIFY(cache1.initialize());
        QVERIFY(cache1.isAvailable());
        QCOMPARE(cache0.get<QString>("str"), "hello");

        // Both caches on this thread should apply the change simultaneously before either emits a signal
        int count = 0;
        auto verify = [&]() {
            QCOMPARE(cache0.get<QString>("str"), "test");
            QCOMPARE(cache1.get<QString>("str"), "test");
            count++;
        };
        connect(&cache0, &DBusWrapper::PropertyCache::propertyChanged, verify);
        connect(&cache1, &DBusWrapper::PropertyCache::propertyChanged, verify);
        service.invoke([](auto s) { s->setStr("test"); });
        QTRY_COMPARE(count, 2);
    }

    void setProperty()
    {
        DBusTest::TestService<PropertyService> service(*dbus);
        DBusWrapper::PropertyCache cache(dbus->client(), testService, testPath, testInterface);
        QTRY_VERIFY(cache.isAvailable());

        // Warning should be printed if the call fails
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("failed to set property \"invalid\""));
        cache.set("invalid", "invalid");

        // Asynchronously call the DBus Set method, wait for the property to actually change
        cache.set("str", "I did it");
        QTRY_COMPARE(cache.get<QString>("str"), "I did it");
    }

    void moveToThread()
    {
        QThread thread;
        thread.start();

        {
            DBusWrapper::PropertyCache cache0(dbus->client(), testService, testPath, testInterface);
            QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("BUG: thread changed"));
            cache0.QObject::moveToThread(&thread);
        }

        thread.quit();
        thread.wait(5000);
    }

    void unexpectedPropertiesChanged()
    {
        // Register the service name, but not the object
        DBusTest::TestService<PropertyService> service(*dbus, [](auto bus) -> auto {
            return new PropertyService(bus, PropertyService::InitMode::NoRegisterObject);
        });

        // Set up a cache and wait for the error
        DBusWrapper::PropertyCache cache(dbus->client(), testService, testPath, testInterface);
        QSignalSpy changeSpy(&cache, &DBusWrapper::PropertyCache::propertyChanged);
        QTRY_COMPARE(cache.error().type(), QDBusError::UnknownObject);

        service.invoke([](auto s) {
            // Register the object (which doesn't emit any signals)
            s->registerObject();
            // Emit a fake signal to the cache that still thinks the object doesn't exist
            DBusWrapper::emitPropertiesChanged(s->m_bus, testPath, testInterface, QVariantMap{{"unexpected", true}});
        });

        // The unexpected signal should cause the cache to retry loading properties, which should
        // succeed now that the object exists.
        QTRY_VERIFY(cache.isAvailable());

        // The property from the fake signal should have been ignored because the cache was unavailable at that time.
        QVERIFY(!cache.contains("unexpected"));
        for (auto p : changeSpy) {
            QVERIFY(p[0].toString() != "unexpected");
            QVERIFY(cache.contains(p[0].toString()));
        }
        QCOMPARE(changeSpy.count(), cache.getAll().count());
    }

private:
    enum InitializationMode
    {
        InitializeNormally,
        InitializeImmediately
    };
    bool expectInitialization(DBusWrapper::PropertyCache* cache, InitializationMode mode)
    {
        expectInitializationImpl(cache, mode);
        return !QTest::currentTestFailed();
    }
    void expectInitializationImpl(DBusWrapper::PropertyCache* cache, InitializationMode mode)
    {
        QVERIFY(!cache->isAvailable());

        // signals should arrive strictly in this order
        int step = 0;
        QList<QMetaObject::Connection> cs;
        cs << connect(cache, &DBusWrapper::PropertyCache::availableChanged, [&](bool available) {
            QCOMPARE(step, 0);
            step++;
            QVERIFY(available);
            QVERIFY(cache->isAvailable());
            QVERIFY(!cache->error().isValid());
            // data should already be available
            QCOMPARE(cache->get<QString>("str"), "hello");
        });
        cs << connect(cache, &DBusWrapper::PropertyCache::propertiesReset, [&](auto properties) {
            QCOMPARE(step, 1);
            step++;
            QCOMPARE(properties, cache->getAll());
        });
        QStringList changeSignals;
        cs << connect(cache, &DBusWrapper::PropertyCache::propertyChanged, [&](auto property, auto value) {
            QCOMPARE(step, 2);
            QCOMPARE(cache->get(property), value);
            QVERIFY(!changeSignals.contains(property));
            changeSignals << property;
        });
        // ready is the last signal
        QSignalSpy spyReady(cache, &DBusWrapper::PropertyCache::ready);

        if (mode == InitializeNormally) {
            QVERIFY(spyReady.wait());
        } else {
            QVERIFY(cache->initialize());
            QCOMPARE(spyReady.count(), 1);
        }

        QCOMPARE(step, 2);
        step++;

        changeSignals.sort();
        QStringList allProperties = cache->getAll().keys();
        allProperties.sort();
        QCOMPARE(changeSignals, allProperties);

        for (auto c : cs)
            disconnect(c);
    }
};

QTEST_MAIN(TestPropertyCache)
#include "tst_propertycache.moc"
