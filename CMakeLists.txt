cmake_minimum_required(VERSION 3.19)

project(dbuswrapper VERSION 1.0.0 DESCRIPTION "Library wrapping the QtDBus bindings to promote sane asynchronous behavior.")
enable_testing()

find_package(Qt5 REQUIRED Core DBus)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libdbustest)
add_subdirectory(tests)

set(PUBLIC_HEADERS
    include/dbuspropertycache.h
    include/dbusadaptorutilities.h
    include/dbusutilities.h
    include/dbustarget.h
)

add_library("${PROJECT_NAME}" SHARED
    src/dbuspropertycache.cpp
    src/dbuspropertycache_p.h
    src/dbusadaptorutilities.cpp
    src/dbusutilities.cpp
    ${PUBLIC_HEADERS}
)

target_include_directories("${PROJECT_NAME}" PUBLIC include)

set_target_properties("${PROJECT_NAME}" PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties("${PROJECT_NAME}" PROPERTIES SOVERSION 1)

# link project against 3rd party libraries
target_link_libraries("${PROJECT_NAME}" PUBLIC Qt5::Core Qt5::DBus)

set_target_properties("${PROJECT_NAME}" PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}" CXX_STANDARD 17)

# installation rule for shared library
include(GNUInstallDirs)
install(TARGETS "${PROJECT_NAME}" LIBRARY PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
