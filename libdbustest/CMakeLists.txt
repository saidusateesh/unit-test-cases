find_package(Qt5 REQUIRED COMPONENTS Test)
set(PUBLIC_HEADERS
    include/testbus.h
    include/testservice.h
)

add_library(dbustest SHARED
    src/testbus.cpp
    src/testservice.cpp
    ${PUBLIC_HEADERS}
)
set_target_properties(dbustest PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)

target_include_directories(dbustest PUBLIC include)
set_target_properties(dbustest PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")
target_link_libraries(dbustest Qt5::Core Qt5::DBus Qt5::Test)

include(GNUInstallDirs)
install(TARGETS dbustest
    LIBRARY
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dbustest")
