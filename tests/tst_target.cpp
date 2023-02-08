#include <QTest>
#include "dbustarget.h"
#include "testbus.h"

static const QString testService = QStringLiteral("test.service");
static const QString testPath = QStringLiteral("/test/path");
static const QString testInterface = QStringLiteral("test.interface");

using namespace DBusWrapper;

class TestTarget : public QObject
{
    Q_OBJECT

private slots:
    void invalid()
    {
        Target invalid;
        QVERIFY(!invalid.isValid());
        QVERIFY(!invalid.bus().isConnected());
        invalid = Target("", testPath, testInterface);
        QVERIFY(!invalid.isValid());
        invalid = Target(testService, "", testInterface);
        QVERIFY(!invalid.isValid());
        invalid = Target(testService, testPath, "");
        QVERIFY(!invalid.isValid());
    }

    void construction()
    {
        Target test(testService, testPath, testInterface);
        QVERIFY(test.isValid());
        QCOMPARE(test.bus().name(), QDBusConnection::sessionBus().name());
        QCOMPARE(test.service(), testService);
        QCOMPARE(test.path(), testPath);
        QCOMPARE(test.interface(), testInterface);

        {
            Target test2(QDBusConnection::sessionBus(), testService, testPath, testInterface);
            QCOMPARE(test, test2);
        }
        {
            Target test3(QDBusConnection::systemBus(), testService, testPath, testInterface);
            QCOMPARE(test3.bus().name(), QDBusConnection::systemBus().name());
        }
    }

    void withFunctions()
    {
        Target test(QDBusConnection::systemBus(), testService, testPath, testInterface);
        {
            Target test2 = test.withPath("/other/path");
            QCOMPARE(test2, Target(QDBusConnection::systemBus(), testService, "/other/path", testInterface));
        }
        {
            Target test3 = test.withInterface("other.interface");
            QCOMPARE(test3, Target(QDBusConnection::systemBus(), testService, testPath, "other.interface"));
        }
        {
            Target test4 = test.with("/other/path", "other.interface");
            QCOMPARE(test4, Target(QDBusConnection::systemBus(), testService, "/other/path", "other.interface"));
        }
    }

    void createMethodCall()
    {
        Target test(testService, testPath, testInterface);
        auto msg = test.createMethodCall("TestMethod");
        QCOMPARE(msg.service(), testService);
        QCOMPARE(msg.path(), testPath);
        QCOMPARE(msg.interface(), testInterface);
        QCOMPARE(msg.member(), "TestMethod");
        QVERIFY(msg.arguments().isEmpty());

        msg = test.createMethodCall("TestMethod", QString("test"), QVariant(1));
        auto args = msg.arguments();
        QVERIFY(args[0].userType() == QMetaType::QString);
        QCOMPARE(args[0], QString("test"));
        // should automatically wrap QVariant in QDBusVariant
        QVERIFY(args[1].userType() == qMetaTypeId<QDBusVariant>());
        QCOMPARE(args[1].value<QDBusVariant>().variant(), QVariant(1));

        // but should not double-wrap if it's already a QDBusVariant
        msg = test.createMethodCall("TestMethod", QVariant::fromValue(QDBusVariant(QVariant(1))));
        auto arg = msg.arguments()[0];
        QCOMPARE(arg.value<QDBusVariant>().variant(), QVariant(1));
    }

    void move()
    {
        Target test(testService, testPath, testInterface);
        Target test2 = test;
        auto other = std::move(test2);
        QVERIFY(!test2.isValid());
        QCOMPARE(test, other);
    }

    void equality()
    {
        QCOMPARE(Target(), Target());
        Target test(QDBusConnection::sessionBus(), testService, testPath, testInterface);
        QCOMPARE(test, test);
        Target test2(QDBusConnection::systemBus(), testService, testPath, testInterface);
        QVERIFY(test != test2);
        Target test3(QDBusConnection::sessionBus(), "other.service", testPath, testInterface);
        QVERIFY(test != test3);
        QVERIFY(test != test.withPath("/other/path"));
        QVERIFY(test != test.withInterface("other.interface"));
    }

    void qhash()
    {
        Target test(testService, testPath, testInterface);
        uint base = qHash(test, 0);
        QVERIFY(base != qHash(test, 1)); // seed is used
        // hash should change if any fields change
        QVERIFY(base != qHash(test.withPath("/other/path"), 0));
        QVERIFY(base != qHash(test.withInterface("other.interface"), 0));
        Target test2("other.service", testPath, testInterface);
        QVERIFY(base != qHash(test2, 0));
        Target test3(QDBusConnection::systemBus(), testService, testPath, testInterface);
        QVERIFY(base != qHash(test3, 0));
    }

    void qdebug()
    {
        Target invalid;
        QString debugStr;
        QDebug(&debugStr) << "a" << invalid << "b";
        QCOMPARE(debugStr, "a DBus(invalid) b ");
        Target test(testService, testPath, testInterface);
        debugStr.clear();
        QDebug(&debugStr) << "a" << test << "b";
        QCOMPARE(debugStr, "a DBus(SessionBus, test.service, /test/path, test.interface) b ");
    }
};

QTEST_MAIN(TestTarget)
#include "tst_target.moc"
