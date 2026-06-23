project(uart_driver)

set(SOURCES
    src/uart_driver.cpp
    src/uart_device.cpp
    src/uart_async.cpp
)

set(HEADERS
    include/uart/uart_driver.h
    include/uart/uart_device.h
    include/uart/uart_async.h
)

add_library(uart_driver ${SOURCES} ${HEADERS})
target_link_libraries(uart_driver pthread)
target_include_directories(uart_driver PUBLIC include)
