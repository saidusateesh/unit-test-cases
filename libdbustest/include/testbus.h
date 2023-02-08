#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QTemporaryDir>
#include <QProcess>
#include <QMutex>
#include <QDeadlineTimer>

namespace DBusTest {

class TestBus : public QObject
{
    Q_OBJECT

public:
    explicit TestBus(QObject* parent = nullptr);
    ~TestBus();

    bool isValid() const;

    QDBusConnection client();
    QDBusConnection getConnection(const QString& name);

    QString busAddress() const;

    bool waitForAllDisconnected(QDeadlineTimer timeout = QDeadlineTimer(5000));

private:
    QTemporaryDir m_tempDir;
    QProcess m_daemon;
    QMutex m_mutex;
    QString m_busAddress;
    QMap<QString,QDBusConnection> m_connections;

    QString writeConfig();
    bool startDaemon(const QString& configPath);
};

}
