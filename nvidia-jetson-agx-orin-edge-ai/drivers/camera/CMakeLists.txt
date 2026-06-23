project(camera_driver)

set(SOURCES
    src/camera_driver.cpp
    src/camera_pipeline.cpp
    src/camera_utils.cpp
)

set(HEADERS
    include/camera/camera_driver.h
    include/camera/camera_pipeline.h
    include/camera/camera_utils.h
)

add_library(camera_driver ${SOURCES} ${HEADERS})
target_link_libraries(camera_driver ${OpenCV_LIBS} ${GSTREAMER_LIBRARIES})
target_include_directories(camera_driver PUBLIC include)
