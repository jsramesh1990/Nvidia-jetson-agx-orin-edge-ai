project(microphone_driver)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)

set(SOURCES
    src/microphone_driver.cpp
    src/audio_processor.cpp
    src/voice_detector.cpp
)

set(HEADERS
    include/microphone/microphone_driver.h
    include/microphone/audio_processor.h
    include/microphone/voice_detector.h
)

add_library(microphone_driver ${SOURCES} ${HEADERS})
target_link_libraries(microphone_driver ${ALSA_LIBRARIES} pthread)
target_include_directories(microphone_driver PUBLIC include ${ALSA_INCLUDE_DIRS})
