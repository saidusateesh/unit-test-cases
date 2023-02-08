#include "testservice.h"

namespace DBusTest {

/*! \internal */
QString uniqueBusName()
{
    static QAtomicInt n;
    return QStringLiteral("unique_%1").arg(n++);
}

}
