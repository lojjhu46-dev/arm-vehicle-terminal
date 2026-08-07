cmake_minimum_required(VERSION 3.5)

set(TARGET myapp)

project(${TARGET})

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
    src/main_weather.c
    src/weather_receiver.c
    src/ui_remote_commands.c
    src/lvgl_file_browser.c
    src/freetype_lcd.c
)

add_subdirectory(${LVGL_ROOT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

add_executable(${TARGET} ${SREC})


target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FREETYPE_ROOT_DIR}/include
)


target_link_directories(${TARGET} PRIVATE
    ${LIB_PATH}
)


target_link_libraries(${TARGET}
    lvgl
    ${FREETYPE_ROOT_DIR}/lib/libfreetype.a
    pthread
    jpeg
    m
    dl
)

if(NOT CMAKE_C_COMPILER MATCHES "arm-linux")
    include(CTest)
    if(BUILD_TESTING)
        add_executable(test_member_login
            tests/test_member_login.c
            src/member_login.c
        )
        target_include_directories(test_member_login PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(test_member_login pthread)
        add_test(NAME member_login COMMAND test_member_login)
    endif()
endif()
