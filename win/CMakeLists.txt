cmake_minimum_required(VERSION 3.24)
project(llc_share_screen)

if (NOT MSVC)
message(FATAL_ERROR "windows platform only")
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_PREFIX_PATH}/bin")

# find uv
find_package(libuv REQUIRED)

# find gl
find_path(GL_INCLUDE_DIR "/GL/glext.h" REQUIRED)

# find avcodec & avutil & swresample
find_path(FFMPEG_INCLUDE_DIR "/libavcodec/codec.h" REQUIRED)

find_library(AVCODEC_DEBUG_LIB NAMES "avcodec_d" "avcodec" REQUIRED)
find_library(AVCODEC_RELEASE_LIB "avcodec" REQUIRED)
add_library(avcodec STATIC IMPORTED)
set_target_properties(avcodec PROPERTIES
    IMPORTED_LOCATION_DEBUG ${AVCODEC_DEBUG_LIB}
    IMPORTED_LOCATION_RELEASE ${AVCODEC_RELEASE_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_DIR})

find_library(AVUTIL_DEBUG_LIB NAMES "avutil_d" "avutil" REQUIRED)
find_library(AVUTIL_RELEASE_LIB "avutil" REQUIRED)
add_library(avutil STATIC IMPORTED)
set_target_properties(avutil PROPERTIES
    IMPORTED_LOCATION_DEBUG ${AVUTIL_DEBUG_LIB}
    IMPORTED_LOCATION_RELEASE ${AVUTIL_RELEASE_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_DIR})

find_library(SWRESAMPLE_DEBUG_LIB NAMES "swresample_d" "swresample" REQUIRED)
find_library(SWRESAMPLE_RELEASE_LIB "swresample" REQUIRED)
add_library(swresample STATIC IMPORTED)
set_target_properties(swresample PROPERTIES
    IMPORTED_LOCATION_DEBUG ${SWRESAMPLE_DEBUG_LIB}
    IMPORTED_LOCATION_RELEASE ${SWRESAMPLE_RELEASE_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_DIR})

set(LLC_SHARE_SCREEN_SRC
    "./src/llc/util/NonCopyable.hpp"
    "./src/llc/util/Singleton.hpp"
    "./src/llc/util/StringView.hpp"
    "./src/llc/util/StringStream.hpp"
    "./src/llc/util/BlockingQueue.hpp"
    "./src/llc/share_screen/pch.hpp"
    "./src/llc/share_screen/common.hpp"
    "./src/llc/share_screen/Render.hpp"
    "./src/llc/share_screen/Render.cpp"
    "./src/llc/share_screen/FrontThread.hpp"
    "./src/llc/share_screen/FrontThread.cpp"
    "./src/llc/share_screen/BackThread.hpp"
    "./src/llc/share_screen/BackThread.cpp"
    "./src/llc/share_screen/DecodeThread.hpp"
    "./src/llc/share_screen/DecodeThread.cpp"
    "./src/llc/share_screen/PtsThread.hpp"
    "./src/llc/share_screen/PtsThread.cpp"
    "./src/llc/share_screen/main.cpp")
add_executable(llc_share_screen ${LLC_SHARE_SCREEN_SRC})
target_include_directories(llc_share_screen PRIVATE ${GL_INCLUDE_DIR} "./src")
target_link_libraries(llc_share_screen PRIVATE uv avcodec avutil swresample opengl32 mfuuid strmiids ole32 user32 winmm d3d11 d3dcompiler)
target_precompile_headers(llc_share_screen PRIVATE "./src/llc/share_screen/pch.hpp")
set_target_properties(llc_share_screen PROPERTIES DEBUG_POSTFIX "_d")
if (DEFINED VS_DEBUG_DIR)
    set_target_properties(llc_share_screen PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${VS_DEBUG_DIR})
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${LLC_SHARE_SCREEN_SRC})