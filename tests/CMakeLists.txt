find_package(Qt5 COMPONENTS Test REQUIRED)

# Required in the github build environment for QThread::create
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

function(add_qt_test)
    set(TEST_NAME ${ARGV0})
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "" "SOURCES")

    add_executable(${TEST_NAME} ${TEST_SOURCES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    target_include_directories(${TEST_NAME} PRIVATE . .. ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE Qt::Core Qt::DBus Qt::Test Threads::Threads dbuswrapper dbustest)
endfunction()

add_qt_test(tst_propertycache SOURCES tst_propertycache.cpp)
add_qt_test(tst_target SOURCES tst_target.cpp)
