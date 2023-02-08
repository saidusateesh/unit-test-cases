//
// Created by dswitz on 8/11/22.
//

#include <QCoreApplication>
#include <gmock/gmock.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testing::InitGoogleTest(&argc, argv);
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
