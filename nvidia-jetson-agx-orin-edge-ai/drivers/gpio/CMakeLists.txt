project(gpio_driver)

set(SOURCES
    src/gpio_driver.cpp
    src/gpio_interrupt.cpp
    src/gpio_pwm.cpp
)

set(HEADERS
    include/gpio/gpio_driver.h
    include/gpio/gpio_interrupt.h
    include/gpio/gpio_pwm.h
)

add_library(gpio_driver ${SOURCES} ${HEADERS})
target_link_libraries(gpio_driver pthread)
target_include_directories(gpio_driver PUBLIC include)
