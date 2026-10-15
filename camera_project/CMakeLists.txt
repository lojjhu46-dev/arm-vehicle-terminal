cmake_minimum_required(VERSION 3.10)
project(camera_video_phone C)

set(CMAKE_C_STANDARD 99)
add_executable(camera_video_phone
    src/main.c
    src/camera_init.c
    src/clinet_and_server.c
    src/frame.c)
target_include_directories(camera_video_phone PRIVATE include)
target_link_libraries(camera_video_phone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libjpeg.a
    pthread)
