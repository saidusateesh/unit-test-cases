#include "testbus.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QLoggingCategory>
#include <QDBusMessage>
#include <QDBusReply>
#include <QTest>

Q_LOGGING_CATEGORY(logTestBus, "libdbustest", QtWarningMsg)

using namespace DBusTest;

/*!
  \class DBusTest::TestBus
  \brief Runs a private DBus instance for unit tests.

  TestBus runs a new isolated instance of \c{dbus-daemon} and creates connections to it. The daemon is set up
  immediately in the constructor and shut down by the destructor. If any error occurs, \l{isValid} will return
  false aftr construction.

  \l{QDBusConnection} instances returned by TestBus are able to communicate with eachother normally, but
  are not affected by any outside activity or configuration.

  It's recommended to use a separate \l{QDBusConnection} for each mock "process". \l{TestService} is useful
  for managing mock service instances.

  \sa TestService
 */

static QString testBusPathTemplate()
{
    auto path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (path.isEmpty())
        path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    path += QStringLiteral("/%1-XXXXXX").arg(QCoreApplication::applicationName());
    return path;
}

/*!
  \brief Creates a new private DBus instance.

  Unit tests should call \c{QVERIFY(dbus.isValid())} after construction in case of any errors.
 */
TestBus::TestBus(QObject* parent)
    : QObject(parent)
    , m_tempDir(testBusPathTemplate())
{
    if (!m_tempDir.isValid()) {
        qCCritical(logTestBus) << "cannot create temporary directory:" << m_tempDir.errorString();
        return;
    }
    QString configPath = writeConfig();
    if (configPath.isEmpty()) {
        qCCritical(logTestBus) << "failed to write config file";
        return;
    }
    if (!startDaemon(configPath)) {
        qCCritical(logTestBus) << "failed to start dbus-daemon";
        return;
    }
    qCDebug(logTestBus) << "launched new dbus-daemon at" << m_busAddress;
}

/*!
  \brief Shuts down the DBus instance.

  All known connections are disconnected immediately, the \c{dbus-daemon} process is terminated, and all temporary
  files are removed. The destructor will wait up to 5 seconds for the process to exit.
 */
TestBus::~TestBus()
{
    qDebug(logTestBus) << "terminating dbus-daemon at" << m_busAddress;
    for (auto &c : m_connections)
        QDBusConnection::disconnectFromBus(c.name());
    m_connections.clear();
    m_daemon.terminate();
    if (!m_daemon.waitForFinished(5000)) {
        qCCritical(logTestBus) << "dbus-daemon process" << m_daemon.processId() << "didn't exit within 5 seconds";
    }
}

/*!
  \brief Waits up to \a{timeout} ms for all connections to disconnect.

  Optionally, tests can call this function to verify that there are no dangling references to any QDBusConnection
  for this bus. Although test buses are isolated, this can be useful to assert that no other state is accidentally
  leaked between tests.

  Note that any existing QDBusConnection instance will hold that connection open.

  Returns true if all connections are disconnected, or false on timeout.
 */
bool TestBus::waitForAllDisconnected(QDeadlineTimer timeout)
{
    auto lastConnection = getConnection("_terminator");
    m_connections.remove(lastConnection.name());
    for (auto &c : m_connections)
        QDBusConnection::disconnectFromBus(c.name());
    QTest::qWait(100);
    m_connections.clear();

    QStringList remaining;
    do {
        auto msg = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
        QDBusReply<QStringList> reply = lastConnection.call(msg, QDBus::Block, timeout.remainingTime());
        remaining = reply.value();
        remaining.removeOne("org.freedesktop.DBus");
        remaining.removeOne(lastConnection.baseService());
        if (remaining.isEmpty()) {
            qDebug(logTestBus) << "all connections have disconnected from" << m_busAddress;
            return true;
        } else {
            QTest::qWait(100);
        }
    } while (!timeout.hasExpired());

    qWarning(logTestBus) << "waitForAllDisconnected timed out with connections remaining:" << remaining;
    return false;
}

/*!
  \brief Returns true if DBus is running successfully.
 */
bool TestBus::isValid() const
{
    return !m_busAddress.isEmpty();
}

/*!
  \brief Returns a \l{QDBusConnection} for client use.

  Equivalent to \l{getConnection}{getConnection("client")}.
 */
QDBusConnection TestBus::client()
{
    return getConnection(QStringLiteral("client"));
}

/*!
  \brief Returns or creates a \l{QDBusConnection} by \a{name}.

  Calling \l{getConnection} on the same TestBus instance with the same name will return the existing connection.
  A new connection is created if necessary. The \a{name} can be any string.

  You should generally use a separate \l{QDBusConnection} for each entity that would be in a separate process.
 */
QDBusConnection TestBus::getConnection(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_connections.find(name);
    if (it != m_connections.end())
        return *it;
    auto c = QDBusConnection::connectToBus(m_busAddress, m_busAddress + ";" + name);
    m_connections.insert(name, c);
    return c;
}

/*!
  \brief Returns the DBus socket address.

  Other processes running as the same user can connect to the DBus instance with this address.
 */
QString TestBus::busAddress() const
{
    return m_busAddress;
}

QString TestBus::writeConfig()
{
    auto path = m_tempDir.filePath("dbus-config.xml");
    QFile dbusConfig(path);
    if (!dbusConfig.open(QIODevice::WriteOnly))
        return {};
    // Raw string literals containing quotes will break moc :/
    dbusConfig.write("\
<busconfig> \
  <type>session</type> \
  <listen>unix:abstract=/tmp/dbus-private</listen> \
  <policy context=\"default\"> \
    <allow send_destination=\"*\"/> \
    <allow eavesdrop=\"true\"/> \
    <allow own=\"*\"/> \
  </policy> \
</busconfig>");
    if (!dbusConfig.flush())
        return {};
    dbusConfig.close();
    return path;
}

bool TestBus::startDaemon(const QString& configPath)
{
    auto socketPath = m_tempDir.filePath("dbus-socket");
    m_daemon.start("dbus-daemon", {
        "--print-address",
        "--nosyslog",
        "--config-file=" + configPath,
        "--address=unix:path=" + socketPath
    });
    m_daemon.waitForReadyRead(5000);
    QString address = m_daemon.readLine().trimmed();
    if (!address.startsWith("unix:"))
        return false;
    m_busAddress = address;
    return true;
}
