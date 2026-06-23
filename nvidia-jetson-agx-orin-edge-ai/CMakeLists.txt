cmake_minimum_required(VERSION 3.10)
project(nvidia-jetson-agx-orin-edge-ai)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wextra -O2")

# Find required packages
find_package(OpenCV REQUIRED)
find_package(CUDA REQUIRED)
find_package(Python3 REQUIRED)
find_package(PkgConfig REQUIRED)

# GStreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)

# ALSA
pkg_check_modules(ALSA REQUIRED alsa)

# Find JSON, YAML, XML libraries
find_package(nlohmann_json QUIET)
find_package(yaml-cpp QUIET)
find_package(TinyXML2 QUIET)

# SSL for encryption
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/ai/face_detection/include
    ${CMAKE_SOURCE_DIR}/ai/face_recognition/include
    ${CMAKE_SOURCE_DIR}/ai/voice_auth/include
    ${CMAKE_SOURCE_DIR}/ai/human_detection/include
    ${CMAKE_SOURCE_DIR}/ai/person_tracking/include
    ${CMAKE_SOURCE_DIR}/ai/attendance/include
    ${CMAKE_SOURCE_DIR}/ai/visitor/include
    ${CMAKE_SOURCE_DIR}/ai/surveillance/include
    ${CMAKE_SOURCE_DIR}/drivers/camera/include
    ${CMAKE_SOURCE_DIR}/drivers/microphone/include
    ${CMAKE_SOURCE_DIR}/drivers/gpio/include
    ${CMAKE_SOURCE_DIR}/drivers/storage/include
    ${CMAKE_SOURCE_DIR}/drivers/spi/include
    ${CMAKE_SOURCE_DIR}/drivers/uart/include
    ${CMAKE_SOURCE_DIR}/drivers/i2c/include
    ${CMAKE_SOURCE_DIR}/communication/mqtt/include
    ${CMAKE_SOURCE_DIR}/communication/tcp/include
    ${CMAKE_SOURCE_DIR}/communication/websocket/include
    ${CMAKE_SOURCE_DIR}/communication/rest_client/include
    ${CMAKE_SOURCE_DIR}/backend/api/include
    ${CMAKE_SOURCE_DIR}/backend/payroll/include
    ${CMAKE_SOURCE_DIR}/backend/analytics/include
    ${CMAKE_SOURCE_DIR}/backend/database/include
    ${CMAKE_SOURCE_DIR}/backend/auth/include
    ${CMAKE_SOURCE_DIR}/backend/notifications/include
    ${CMAKE_SOURCE_DIR}/third_party/include
)

# Link directories
link_directories(
    ${CMAKE_SOURCE_DIR}/third_party/lib
)

# Add subdirectories (only if they exist)
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/face_detection/CMakeLists.txt)
    add_subdirectory(ai/face_detection)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/face_recognition/CMakeLists.txt)
    add_subdirectory(ai/face_recognition)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/voice_auth/CMakeLists.txt)
    add_subdirectory(ai/voice_auth)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/human_detection/CMakeLists.txt)
    add_subdirectory(ai/human_detection)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/person_tracking/CMakeLists.txt)
    add_subdirectory(ai/person_tracking)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/attendance/CMakeLists.txt)
    add_subdirectory(ai/attendance)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/visitor/CMakeLists.txt)
    add_subdirectory(ai/visitor)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/ai/surveillance/CMakeLists.txt)
    add_subdirectory(ai/surveillance)
endif()

# Driver subdirectories
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/camera/CMakeLists.txt)
    add_subdirectory(drivers/camera)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/microphone/CMakeLists.txt)
    add_subdirectory(drivers/microphone)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/gpio/CMakeLists.txt)
    add_subdirectory(drivers/gpio)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/storage/CMakeLists.txt)
    add_subdirectory(drivers/storage)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/spi/CMakeLists.txt)
    add_subdirectory(drivers/spi)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/uart/CMakeLists.txt)
    add_subdirectory(drivers/uart)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/drivers/i2c/CMakeLists.txt)
    add_subdirectory(drivers/i2c)
endif()

# Communication subdirectories
if(EXISTS ${CMAKE_SOURCE_DIR}/communication/mqtt/CMakeLists.txt)
    add_subdirectory(communication/mqtt)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/communication/tcp/CMakeLists.txt)
    add_subdirectory(communication/tcp)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/communication/websocket/CMakeLists.txt)
    add_subdirectory(communication/websocket)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/communication/rest_client/CMakeLists.txt)
    add_subdirectory(communication/rest_client)
endif()

# Backend subdirectories
if(EXISTS ${CMAKE_SOURCE_DIR}/backend/api/CMakeLists.txt)
    add_subdirectory(backend/api)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/backend/payroll/CMakeLists.txt)
    add_subdirectory(backend/payroll)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/backend/analytics/CMakeLists.txt)
    add_subdirectory(backend/analytics)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/backend/database/CMakeLists.txt)
    add_subdirectory(backend/database)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/backend/auth/CMakeLists.txt)
    add_subdirectory(backend/auth)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/backend/notifications/CMakeLists.txt)
    add_subdirectory(backend/notifications)
endif()

# Main executable
add_executable(edge_ai_app main.cpp)

target_link_libraries(edge_ai_app 
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
    curl
    mosquitto
    sqlite3
)

# Link driver libraries if they exist
target_link_libraries(edge_ai_app
    camera_driver
    microphone_driver
    gpio_driver
    storage_driver
    spi_driver
    uart_driver
    i2c_driver
)

# Install targets
install(TARGETS edge_ai_app DESTINATION bin)
install(DIRECTORY configs/ DESTINATION etc/edge_ai)
install(DIRECTORY data/ DESTINATION var/edge_ai)
install(DIRECTORY scripts/ DESTINATION usr/local/bin/edge_ai)
