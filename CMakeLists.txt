cmake_minimum_required(VERSION 3.16)
project(hms_firetv CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# ── Options ────────────────────────────────────────────────────────────────────
option(BUILD_WITH_POSTGRESQL "Enable PostgreSQL support" ON)
option(BUILD_TESTS "Build unit tests" ON)

# ── Required packages ──────────────────────────────────────────────────────────
find_package(Drogon CONFIG REQUIRED)
find_package(jsoncpp REQUIRED)
find_library(JSONCPP_LIB jsoncpp)
if(NOT JSONCPP_LIB)
    message(FATAL_ERROR "libjsoncpp not found. Install with: apt-get install libjsoncpp-dev")
endif()
find_package(eclipse-paho-mqtt-c REQUIRED)
find_package(PahoMqttCpp REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Paho C++ wrapper (prefer /usr/local for source builds)
find_library(PAHO_MQTTPP3_LIB paho-mqttpp3 PATHS /usr/local/lib NO_DEFAULT_PATH)
if(NOT PAHO_MQTTPP3_LIB)
    find_library(PAHO_MQTTPP3_LIB paho-mqttpp3)
endif()

# ── SQLite3 — always required (default database) ───────────────────────────────
find_library(SQLITE3_LIB sqlite3 REQUIRED)
if(NOT SQLITE3_LIB)
    message(FATAL_ERROR "libsqlite3 not found. Install with: apt-get install libsqlite3-dev")
endif()

# ── PostgreSQL — optional ──────────────────────────────────────────────────────
if(BUILD_WITH_POSTGRESQL)
    find_library(PQXX_LIB pqxx)
    find_library(PQ_LIB pq)
    if(NOT PQXX_LIB)
        message(WARNING "libpqxx not found — disabling PostgreSQL support")
        set(BUILD_WITH_POSTGRESQL OFF)
    else()
        add_definitions(-DWITH_POSTGRESQL)
        find_path(PQ_INCLUDE_DIR libpq-fe.h PATHS /usr/include/postgresql)
        if(PQ_INCLUDE_DIR)
            include_directories(${PQ_INCLUDE_DIR})
        endif()
    endif()
endif()

# ── Include directories ────────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})

# ── Sources ────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/**/*.cpp
)

# Exclude PostgresDatabase if not building with PostgreSQL
if(NOT BUILD_WITH_POSTGRESQL)
    list(FILTER SOURCES EXCLUDE REGEX ".*PostgresDatabase\\.cpp$")
endif()

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

# ── Link libraries ─────────────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME}
    Drogon::Drogon
    ${JSONCPP_LIB}
    eclipse-paho-mqtt-c::paho-mqtt3as
    ${PAHO_MQTTPP3_LIB}
    ${SQLITE3_LIB}
    ${CURL_LIBRARIES}
    pthread
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(BUILD_WITH_POSTGRESQL)
    target_link_libraries(${PROJECT_NAME} ${PQXX_LIB} ${PQ_LIB})
endif()

# ── Install ────────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# ── Tests ──────────────────────────────────────────────────────────────────────
if(BUILD_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()
