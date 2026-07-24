cmake_minimum_required(VERSION 3.5)

set(TARGET myapp)

project(${TARGET})

set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(SREC
    src/main.c
    src/lcd.c 
    src/touch.c 
    src/camera.c 
    src/image_convert.c 
    src/live_controls.c
    src/overlay.c 
    src/record_indicator.c 
    src/jpeg_codec.c 
    src/recorder.c 
    src/ui_flow.c    
)

add_executable(${TARGET} ${SREC})


target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_directories(${TARGET} PRIVATE
    ${LIB_PATH}
)


target_link_libraries(${TARGET}
    pthread
    jpeg
)
