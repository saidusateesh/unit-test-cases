## TL;DR
Library wrapping QtDBus bindings intended to be used in place of QtDBus to avoid common pitfalls.

## What?
#### Problem:
QtDBus provides [tooling to auto generate classes](https://doc.qt.io/qt-5/qdbusxml2cpp.html) for both the consumer and producer side of a given DBus API. Classes generated for the consumer side inherit from [`QDBusAbstractInterface`](https://doc.qt.io/qt-5/qdbusabstractinterface.html) which provides a convenient API to invoke methods, retrieve properties, and receive signals from the underlying DBus interface.

However, the implementation of `QDBusAbstractInterface` (as well as `QDBusInterface`) creates blocking calls when fetching and setting property values. Any time the client wishes to obtain the value of a property, it will be blocked until a reply is received from the corresponding DBus service (or an error message is recieved from the DBus Daemon itself).

Furthermore, Qt _does not_ support the standard [`org.freedesktop.DBus.Properties.PropertiesChanged`](https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties) signal: [https://bugreports.qt.io/browse/QTBUG-48008](https://bugreports.qt.io/browse/QTBUG-48008) This forces individuals to either implement support themselves or make individual change signals for each property.

#### Solution:
This Library!

##### Todo:
- [ ] Explain what `libdbuswrapper` provides
- [ ] Provide examples


## Build & Installation (on host machine)
```
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=`qmake -query QT_HOST_PREFIX` -DCMAKE_INSTALL_PREFIX=`qmake -query QT_HOST_PREFIX` ..
make install
```
