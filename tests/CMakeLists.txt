cmake_minimum_required(VERSION 3.16)

find_package(GTest REQUIRED)
find_package(GMock QUIET)
find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

# SQLite3 always needed
find_library(SQLITE3_LIB sqlite3 REQUIRED)

# PostgreSQL optional (inherited from parent)
set(PG_LIBS "")
if(BUILD_WITH_POSTGRESQL)
    set(PG_LIBS ${PQXX_LIB} ${PQ_LIB})
endif()

set(DB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/database/SQLiteDatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/AppConfig.cpp
)
if(BUILD_WITH_POSTGRESQL)
    list(APPEND DB_SOURCES ${CMAKE_SOURCE_DIR}/src/database/PostgresDatabase.cpp)
endif()

set(TEST_SOURCES
    test_device_controller.cpp
    test_command_controller.cpp
    test_command_handler.cpp
    test_mqtt_client.cpp
    test_pairing_controller.cpp
    test_apps_controller.cpp
    test_stats_controller.cpp
    test_apps_repository.cpp
)

set(UNIT_TEST_SOURCES
    test_background_logger.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name}
        ${test_src}
        ${DB_SOURCES}
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
        ${CMAKE_SOURCE_DIR}/src/services/DiscoveryService.cpp
    )

    target_link_libraries(${test_name}
        ${GTEST_BOTH_LIBRARIES}
        Threads::Threads
        Drogon::Drogon
        ${JSONCPP_LIB}
        eclipse-paho-mqtt-c::paho-mqtt3as
        ${PAHO_MQTTPP3_LIB}
        ${SQLITE3_LIB}
        ${PG_LIBS}
        ${CURL_LIBRARIES}
        pthread
        OpenSSL::SSL
        OpenSSL::Crypto
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_src ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name} ${test_src})

    target_link_libraries(${test_name}
        ${GTEST_BOTH_LIBRARIES}
        Threads::Threads
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

enable_testing()
