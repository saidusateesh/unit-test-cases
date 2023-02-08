#pragma once

#include "testbus.h"
#include <QDBusConnection>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <functional>

namespace DBusTest
{

extern QString uniqueBusName();

/*!
  \class DBusTest::TestService
  \brief Manages a mock DBus service on a separate thread.

  TestService is a unit testing utility to run and communicate with mock services in a separate thread.

  Constructing a \c{TestService<Controller>} will:
  \list
    \li Create a new DBus connection to the specified \l{TestBus} or standard bus
    \li Create a new thread
    \li In the new thread, create the controller object from the template type \c{Controller}
    \li Return from the constructor once fully initialized
  \endlist

  On destruction, it will:
  \list
    \li Destroy the controller object (on the service thread)
    \li Disconnect the DBus connection
    \li Stop the thread
    \li Return from the destructor once the thread has stopped
  \endlist

  The \c{Controller} type implements your API for interacting with your mock services. The only requirement
  is that it must be a \l{QObject}. The object is created by the \e{init} function passed to the TestService
  constructor. The default init function calls \c{new Controller(bus)} (passing a \l{QDBusConnection}).

  The controller can't be accessed directly because it lives on a separate thread. The \l{invoke} method
  asynchronously runs a function on the thread with access to the controller:

  \code
  void testInvoke()
  {
      DBusTest::TestService<MockExampleController> mock(dbus);
      QVERIFY(mock.isValid());

      // ExampleClient connects to the dbus signal
      ExampleClient client(dbus.client());

      // Call MockExampleController::emitSignal() from the service thread
      mock.invoke([](auto controller) { controller->emitSignal(); });

      // Process events until ExampleClient::signalReceived is true
      QTRY_VERIFY(client.signalReceived);
  }
  \endcode

  The \l{sync} method blocks the calling thread until the function returns on the service's thread. It's
  safe to use data and objects from the calling thread during \l{sync}:

  \code
  void testSync()
  {
      DBusTest::TestService<MockExampleController> mock(dbus);
      QVERIFY(mock.isValid());

      QUuid expectedUuid;
      mock.sync([&](auto controller) { expectedUuid = controller->serviceUuid(); });
      // ...
  }
  \endcode
 */

template<typename Controller> class TestService
{
public:
    /*!
      \brief Constructs TestService with a new connection from \a{testBus}.

      The \c{Controller} type must have a constructor matching \c{Controller::Controller(QDBusConnection)}.
     */
    explicit TestService(TestBus& testBus)
        : TestService(QDBusConnection::connectToBus(testBus.busAddress(), uniqueBusName()))
    {
    }

    /*!
      \brief Constructs TestService with a new connection from \a{testBus} and custom \a{init} function.
     */
    TestService(TestBus& testBus, std::function<Controller*(QDBusConnection)> init)
        : TestService(QDBusConnection::connectToBus(testBus.busAddress(), uniqueBusName()), init)
    {
    }

    /*!
      \brief Constructs TestService with a new connection to the well-known bus \a{type}.

      The \c{Controller} type must have a constructor matching \c{Controller::Controller(QDBusConnection)}.
     */
    explicit TestService(QDBusConnection::BusType type)
        : TestService(QDBusConnection::connectToBus(type, uniqueBusName()))
    {
    }

    /*!
      \brief Constructs TestService with a new connection to the well-known bus \a{type} and custom \a{init} function.
     */
    TestService(QDBusConnection::BusType type, std::function<Controller*(QDBusConnection)> init)
        : TestService(QDBusConnection::connectToBus(type, uniqueBusName()), init)
    {
    }

    /*!
      \brief Deletes the controller object, disconnects the DBus connection, and stops the thread.
     */
    ~TestService()
    {
        if (m_controller) {
            sync([&](auto) {
                delete m_controller;
                m_controller = nullptr;
                QDBusConnection::disconnectFromBus(m_bus.name());
            });
        }
        m_thread.quit();
        m_thread.wait();
    }

    /*!
      \brief Returns true if the \c{Controller} was created successfully.
     */
    bool isValid() const
    {
        return static_cast<bool>(m_controller);
    }

    /*!
      \brief Returns the service's private DBus connection.
     */
    QDBusConnection bus() const
    {
        return m_bus;
    }

    /*!
      \brief Queues a call of the function \a{f} on the service's thread and returns immediately.

      \note
      Be sure to capture by-value when invoking a lambda. Values may change on the calling thread before
      the function is invoked.
     */
    void invoke(std::function<void(Controller*)> f)
    {
        Q_ASSERT(m_thread.isRunning());
        QMetaObject::invokeMethod(m_controller, [=]() {
            f(m_controller);
        }, Qt::QueuedConnection);
    }

    /*!
      \brief Calls the function \a{f} on the service's thread and waits for it to return.

      Because the calling thread will be blocked, it's safe to use values and objects from that thread,
      including lambda values by-reference.

      \note
      Any QObject created inside the function will live on the service thread, not the calling thread.
     */
    void sync(std::function<void(Controller*)> f)
    {
        Q_ASSERT(m_thread.isRunning());
        QMetaObject::invokeMethod(m_controller, [=]() {
            f(m_controller);
        }, Qt::BlockingQueuedConnection);
    }

private:
    QDBusConnection m_bus;
    QThread m_thread;
    Controller* m_controller = nullptr;

    /*! \internal */
    explicit TestService(const QDBusConnection& bus)
        : TestService(bus, [](auto bus) -> auto { return new Controller(bus); })
    {
    }

    /*! \internal */
    TestService(const QDBusConnection& bus, std::function<Controller*(QDBusConnection)> init)
        : m_bus(bus)
    {
        m_thread.setObjectName(Controller::staticMetaObject.className());

        QMutex mutex;
        QWaitCondition wait;
        auto c = QObject::connect(&m_thread, &QThread::started, [&]() {
            QMutexLocker lock(&mutex);
            m_controller = init(m_bus);
            wait.wakeAll();
        });

        QMutexLocker lock(&mutex);
        m_thread.start();
        wait.wait(&mutex);
        QObject::disconnect(c);
    }
};

}
