project(spi_driver)

set(SOURCES
    src/spi_driver.cpp
    src/spi_device.cpp
    src/spi_bus.cpp
)

set(HEADERS
    include/spi/spi_driver.h
    include/spi/spi_device.h
    include/spi/spi_bus.h
)

add_library(spi_driver ${SOURCES} ${HEADERS})
target_link_libraries(spi_driver)
target_include_directories(spi_driver PUBLIC include)
