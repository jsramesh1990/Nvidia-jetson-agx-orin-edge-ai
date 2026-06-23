project(face_detection)

set(SOURCES
    src/detector.cpp
    src/model_loader.cpp
    src/post_processing.cpp
)

set(HEADERS
    include/face_detection/detector.h
    include/face_detection/model_loader.h
    include/face_detection/post_processing.h
)

add_library(face_detection ${SOURCES} ${HEADERS})
target_link_libraries(face_detection ${OpenCV_LIBS} ${CUDA_LIBRARIES})
target_include_directories(face_detection PUBLIC include)
