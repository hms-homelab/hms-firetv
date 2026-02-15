cmake_minimum_required(VERSION 3.16)

# Find GoogleTest
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

# Test source files (full integration tests with dependencies)
set(TEST_SOURCES
    test_device_controller.cpp
    test_command_controller.cpp
    test_pairing_controller.cpp
    test_apps_controller.cpp
    test_stats_controller.cpp
    test_apps_repository.cpp
)

# Unit tests that don't need full dependencies
set(UNIT_TEST_SOURCES
    test_background_logger.cpp
)

# Create test executable for each test file
foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name}
        ${test_src}
        ${CMAKE_SOURCE_DIR}/src/api/DeviceController.cpp
        ${CMAKE_SOURCE_DIR}/src/api/CommandController.cpp
        ${CMAKE_SOURCE_DIR}/src/api/PairingController.cpp
        ${CMAKE_SOURCE_DIR}/src/api/AppsController.cpp
        ${CMAKE_SOURCE_DIR}/src/api/StatsController.cpp
        ${CMAKE_SOURCE_DIR}/src/repositories/DeviceRepository.cpp
        ${CMAKE_SOURCE_DIR}/src/repositories/AppsRepository.cpp
        ${CMAKE_SOURCE_DIR}/src/services/DatabaseService.cpp
        ${CMAKE_SOURCE_DIR}/src/clients/LightningClient.cpp
        ${CMAKE_SOURCE_DIR}/src/mqtt/CommandHandler.cpp
        ${CMAKE_SOURCE_DIR}/src/mqtt/MQTTClient.cpp
        ${CMAKE_SOURCE_DIR}/src/mqtt/DiscoveryPublisher.cpp
    )

    target_link_libraries(${test_name}
        ${GTEST_BOTH_LIBRARIES}
        Threads::Threads
        Drogon::Drogon
        jsoncpp_lib
        eclipse-paho-mqtt-c::paho-mqtt3as
        /usr/local/lib/libpaho-mqttpp3.so
        ${PQXX_LIB}
        ${PQ_LIB}
        ${CURL_LIBRARIES}
        pthread
        ssl
        crypto
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Create test executables for unit tests (minimal dependencies)
foreach(test_src ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name}
        ${test_src}
    )

    target_link_libraries(${test_name}
        ${GTEST_BOTH_LIBRARIES}
        Threads::Threads
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Enable testing
enable_testing()
