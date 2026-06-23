project(storage_driver)

set(SOURCES
    src/storage_driver.cpp
    src/storage_manager.cpp
    src/file_system.cpp
)

set(HEADERS
    include/storage/storage_driver.h
    include/storage/storage_manager.h
    include/storage/file_system.h
)

add_library(storage_driver ${SOURCES} ${HEADERS})
target_link_libraries(storage_driver pthread)
target_include_directories(storage_driver PUBLIC include)
