cmake_minimum_required(VERSION 3.16)
project(hms_firetv CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Find required packages
find_package(Drogon CONFIG REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(eclipse-paho-mqtt-c REQUIRED)  # Find C library first
find_package(PahoMqttCpp REQUIRED)
find_package(CURL REQUIRED)

# Find libpqxx (may need manual path if not in standard location)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
if(NOT PQXX_LIB)
    message(FATAL_ERROR "libpqxx not found. Install with: apt-get install libpqxx-dev")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})

# Source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/**/*.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Drogon::Drogon
    jsoncpp_lib
    eclipse-paho-mqtt-c::paho-mqtt3as  # C library with SSL (async)
    /usr/local/lib/libpaho-mqttpp3.so  # C++ wrapper (direct path to avoid broken CMake config)
    ${PQXX_LIB}
    ${PQ_LIB}
    ${CURL_LIBRARIES}
    pthread
    ssl
    crypto
)

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)
