#include "dbuspropertycache.h"
#include "dbustarget.h"
#include <gtest/gtest.h>
#include <QtDBus/QDBusConnection>

class PropertyCacheTests : public ::testing::Test {
protected:
    const DBusWrapper::Target *target_;
    DBusWrapper::PropertyCache *propertyCache_;
    const QDBusConnection *bus_;

public:
    PropertyCacheTests()
    {

    }

    virtual void SetUp() override{
        //    PropertyCache(const QString& service, const QString& path, const QString& interface, QObject *parent = nullptr);

    	bus_ = new QDBusConnection("TestConnection");
        target_ = new DBusWrapper::Target(*bus_, "dbuswrappertest", "/home/ubuntu/src/libdbuswrapper-master/unit-tests", "sample");
    
        //propertyCache_ = new DBusWrapper::PropertyCache("dbuswrappertest", "/home/ubuntu/src/libdbuswrapper-master/unit-tests", "dbuswrapperTest");
        propertyCache_ = new DBusWrapper::PropertyCache(*target_);
    }

    virtual void TearDown() override{
    	delete bus_;
        delete target_;
        delete propertyCache_;
    }
};

TEST_F(PropertyCacheTests, initialize)
{
    EXPECT_EQ(propertyCache_->initialize(), false);
}
/*

TEST_F(PropertyCacheTests, contains)
{
    QVariant property = propertyCache_->get();
	propertyCache_->availableChanged(true);
	EXPECT_EQ(propertyCache_->contains(), true);
}*/

TEST_F(PropertyCacheTests, target)
{
    const DBusWrapper::Target target = propertyCache_->target();
    EXPECT_EQ(target, *target_);
}

/*TEST_F(PropertyCacheTests, bus)
{
    const QDBusConnection bus = propertyCache_->bus();

    EXPECT_EQ(bus, *bus_);
}*/

TEST_F(PropertyCacheTests, get)
{
    const QString property = "Test"; 
    const QVariant value = 3.0;
    propertyCache_->set(property, value);
    EXPECT_EQ(value, propertyCache_->get("Test"));

}


