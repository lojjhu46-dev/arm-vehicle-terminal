cmake_minimum_required(VERSION 3.15)

set(TARGET myapp)

project(${TARGET})

# CMake's default Makefile clean target removes target outputs only.  Include
# its generated metadata as well so `make clean` returns the build directory
# to an unconfigured state without removing the directory itself.
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/CMakeCache.txt"
    "${CMAKE_BINARY_DIR}/CMakeFiles"
    "${CMAKE_BINARY_DIR}/Makefile"
    "${CMAKE_BINARY_DIR}/cmake_install.cmake"
    "${CMAKE_BINARY_DIR}/CTestTestfile.cmake"
    "${CMAKE_BINARY_DIR}/DartConfiguration.tcl"
    "${CMAKE_BINARY_DIR}/Testing"
    "${CMAKE_BINARY_DIR}/lvgl"
)

set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(FREETYPE_ROOT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../freetype/freetype_arm"
    CACHE PATH "FreeType ARM SDK directory")
if(NOT EXISTS "${FREETYPE_ROOT_DIR}/include/ft2build.h" OR
   NOT EXISTS "${FREETYPE_ROOT_DIR}/lib/libfreetype.a")
    message(FATAL_ERROR "FreeType ARM SDK not found: ${FREETYPE_ROOT_DIR}")
endif()

# LVGL is used only by the file browser. The default points at the local
# reference checkout, while cross builds may provide another checkout.
set(LVGL_ROOT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../lvgl_project/lv_port_pc_vscode/lvgl"
    CACHE PATH "LVGL source directory")
if(NOT IS_DIRECTORY "${LVGL_ROOT_DIR}")
    message(FATAL_ERROR "LVGL source directory not found: ${LVGL_ROOT_DIR}")
endif()
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
    CACHE PATH "LVGL configuration file")
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(SREC
    src/main.c
    src/lcd.c 
    src/touch.c 
    src/camera.c 
    src/image_convert.c 
    src/live_controls.c
    src/playback.c
    src/overlay.c 
    src/record_indicator.c 
    src/jpeg_codec.c 
    src/member_login.c
    src/member_sms.c
    src/recorder.c 
    src/ui_flow.c
    src/udp_sender.c
    src/main_weather.c
    src/weather_receiver.c
    src/ui_remote_commands.c
    src/lvgl_runtime.c
    src/lvgl_ui.c
    src/lvgl_file_browser.c
    src/freetype_lcd.c
)

add_subdirectory(${LVGL_ROOT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

target_include_directories(lvgl PRIVATE
    ${FREETYPE_ROOT_DIR}/include
)

add_executable(${TARGET} ${SREC})


target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FREETYPE_ROOT_DIR}/include
)


target_link_directories(${TARGET} PRIVATE
    ${LIB_PATH}
)


target_link_libraries(${TARGET}
    "-Wl,--whole-archive"
    lvgl
    "-Wl,--no-whole-archive"
    ${FREETYPE_ROOT_DIR}/lib/libfreetype.a
    pthread
    jpeg
    m
    dl
)

if(NOT CMAKE_C_COMPILER MATCHES "arm-linux")
    include(CTest)
    if(BUILD_TESTING)
        set(HOST_JPEG_ROOT_DIR "/home/gec/workspace/libjpeg-gcc"
            CACHE PATH "Host libjpeg installation used by JPEG-dependent tests")
        add_executable(test_member_login
            tests/test_member_login.c
            src/member_login.c
        )
        target_include_directories(test_member_login PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(test_member_login pthread)
        add_test(NAME member_login COMMAND test_member_login)

        add_executable(test_weather_receiver
            tests/test_weather_receiver.c
            src/weather_receiver.c
            src/ui_remote_commands.c
        )
        target_include_directories(test_weather_receiver PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(test_weather_receiver pthread m)
        add_test(NAME weather_receiver COMMAND test_weather_receiver)

        add_executable(test_touch_pointer
            tests/test_touch_pointer.c
            src/touch.c
        )
        target_include_directories(test_touch_pointer PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(test_touch_pointer pthread)
        add_test(NAME touch_pointer COMMAND test_touch_pointer)

        add_executable(test_live_controls
            tests/test_live_controls.c
            src/live_controls.c
        )
        target_include_directories(test_live_controls PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        add_test(NAME live_controls COMMAND test_live_controls)

        add_executable(test_ui_remote_commands
            tests/test_ui_remote_commands.c
            src/ui_remote_commands.c
        )
        target_include_directories(test_ui_remote_commands PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(test_ui_remote_commands pthread)
        add_test(NAME ui_remote_commands COMMAND test_ui_remote_commands)

        find_library(HOST_FREETYPE_LIBRARY NAMES freetype)
        if(NOT HOST_FREETYPE_LIBRARY)
            find_file(HOST_FREETYPE_RUNTIME_LIBRARY NAMES libfreetype.so.6
                PATHS /lib /usr/lib
                PATH_SUFFIXES x86_64-linux-gnu i386-linux-gnu)
            set(HOST_FREETYPE_LIBRARY "${HOST_FREETYPE_RUNTIME_LIBRARY}")
        endif()
        if(HOST_FREETYPE_LIBRARY)
            add_executable(test_lvgl_ui
                tests/test_lvgl_ui.c
                src/lvgl_runtime.c
                src/lvgl_ui.c
                src/main_weather.c
                src/jpeg_codec.c
                src/lcd.c
                src/touch.c
                src/freetype_lcd.c
                src/member_login.c
                src/member_sms.c
            )
            target_include_directories(test_lvgl_ui PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${FREETYPE_ROOT_DIR}/include
                ${HOST_JPEG_ROOT_DIR}/include
                ${LVGL_ROOT_DIR}
            )
            target_link_directories(test_lvgl_ui PRIVATE
                ${HOST_JPEG_ROOT_DIR}/lib
            )
            target_link_libraries(test_lvgl_ui
                "-Wl,--whole-archive"
                lvgl
                "-Wl,--no-whole-archive"
                ${HOST_FREETYPE_LIBRARY}
                pthread
                jpeg
                m
                dl
            )
            set_target_properties(test_lvgl_ui PROPERTIES
                BUILD_RPATH "${HOST_JPEG_ROOT_DIR}/lib"
            )
            add_test(NAME lvgl_ui COMMAND test_lvgl_ui)
            set_tests_properties(lvgl_ui PROPERTIES
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        else()
            message(WARNING "Host FreeType not found; skipping lvgl_ui test.")
        endif()

        if(EXISTS "${HOST_JPEG_ROOT_DIR}/include/jpeglib.h" AND
           EXISTS "${HOST_JPEG_ROOT_DIR}/lib/libjpeg.so")
            add_executable(test_main_weather
                tests/test_main_weather.c
                src/main_weather.c
                src/jpeg_codec.c
                src/lcd.c
            )
            target_include_directories(test_main_weather PRIVATE
                ${HOST_JPEG_ROOT_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/include
            )
            target_link_directories(test_main_weather PRIVATE
                ${HOST_JPEG_ROOT_DIR}/lib
            )
            target_link_libraries(test_main_weather pthread jpeg)
            set_target_properties(test_main_weather PROPERTIES
                BUILD_RPATH "${HOST_JPEG_ROOT_DIR}/lib"
            )
            add_test(NAME main_weather COMMAND test_main_weather)
            set_tests_properties(main_weather PROPERTIES
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        else()
            message(WARNING "Host libjpeg not found; skipping main_weather test. "
                            "Set HOST_JPEG_ROOT_DIR to enable it.")
        endif()
    endif()
endif()
