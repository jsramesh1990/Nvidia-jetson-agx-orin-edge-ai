project(i2c_driver)

set(SOURCES
    src/i2c_driver.cpp
    src/i2c_device.cpp
    src/i2c_bus.cpp
)

set(HEADERS
    include/i2c/i2c_driver.h
    include/i2c/i2c_device.h
    include/i2c/i2c_bus.h
)

add_library(i2c_driver ${SOURCES} ${HEADERS})
target_link_libraries(i2c_driver)
target_include_directories(i2c_driver PUBLIC include)
