cmake_minimum_required(VERSION 3.20)

project(share_screen)

################################################################################
# read version name and code from version.txt.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../version.txt" _VERSION_STRINGS)
list(GET _VERSION_STRINGS 0 _VERSION_STRING)
string(REGEX REPLACE "name=([0-9]+\\.[0-9]+\\.[0-9]+) code=([0-9]+)" "\\1" SS_VERSION_NAME "${_VERSION_STRING}")
string(REGEX REPLACE "name=([0-9]+\\.[0-9]+\\.[0-9]+) code=([0-9]+)" "\\2" SS_VERSION_CODE "${_VERSION_STRING}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" SS_VERSION_MAJOR "${SS_VERSION_NAME}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2" SS_VERSION_MINOR "${SS_VERSION_NAME}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3" SS_VERSION_PATCH "${SS_VERSION_NAME}")
message(STATUS "version name: ${SS_VERSION_NAME}")
message(STATUS "version code: ${SS_VERSION_CODE}")
message(STATUS "version major: ${SS_VERSION_MAJOR}")
message(STATUS "version minor: ${SS_VERSION_MINOR}")
message(STATUS "version patch: ${SS_VERSION_PATCH}")
################################################################################

################################################################################
# system diff specs.
if (CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    list(APPEND SS_DEFS SS_IS_LITTLE)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    list(APPEND SS_SRC
        ./src/ss/_resource.h
        ./src/ss/_ss.rc
        ./src/ss/main_thread_impl/Windows.hpp
        ./src/ss/main_thread_impl/Windows.cpp
    )
    list(APPEND SS_LINK_LIBS bcrypt)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_link_options("-static-libgcc" "-static-libstdc++")
    SET(CMAKE_SKIP_BUILD_RPATH FALSE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    SET(CMAKE_INSTALL_RPATH "$ORIGIN")

    list(APPEND SS_SRC
        ./src/ss/main_thread_impl/Linux.hpp
        ./src/ss/main_thread_impl/Linux.cpp
    )
	find_package(X11 REQUIRED)
    list(APPEND SS_LINK_LIBS X11::X11 m)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	SET(CMAKE_SKIP_BUILD_RPATH FALSE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    SET(CMAKE_MACOSX_RPATH TRUE)
	set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)
	SET(CMAKE_INSTALL_RPATH "@executable_path")

    list(APPEND SS_SRC
        ./src/ss/main_thread_impl/MacOs.hpp
        ./src/ss/main_thread_impl/MacOs.mm
    )
    list(APPEND SS_LINK_LIBS "-framework Cocoa" "m")
endif()
################################################################################

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
cmake_policy(SET CMP0068 NEW)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_DEBUG_POSTFIX _d)
set(FFMPEG_INSTALL_PATH "" CACHE PATH "ffmpeg install path")
list(APPEND CMAKE_PREFIX_PATH "${FFMPEG_INSTALL_PATH}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SS_ENABLE_ASAN OFF CACHE BOOL "enable address sanitize(only for linux)")
if (SS_ENABLE_ASAN)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND CMAKE_C_FLAGS "-fno-omit-frame-pointer" "-fsanitize=address")
        list(APPEND CMAKE_CXX_FLAGS "-fno-omit-frame-pointer" "-fsanitize=address")
    endif()
endif()

include(FetchContent)

################################################################################
# libuv...
set(LIBUV_BUILD_SHARED OFF CACHE INTERNAL "")
set(ASAN OFF CACHE INTERNAL "")
set(MSAN OFF CACHE INTERNAL "")
set(TSAN OFF CACHE INTERNAL "")
set(UBSAN OFF CACHE INTERNAL "")
set(QEMU OFF CACHE INTERNAL "")
FetchContent_Declare(
    libuv
    URL "${CMAKE_CURRENT_SOURCE_DIR}/3rd/libuv-1.40.0.zip"
)
FetchContent_MakeAvailable(libuv)
set_target_properties(uv_a uv PROPERTIES FOLDER 3rd)
################################################################################

################################################################################
# ffmpeg...
include(ffmpeg_target)
ffmpeg_target(avcodec /libavcodec/version_major.h)
ffmpeg_target(avutil /libavutil/version.h)
ffmpeg_target(swresample /libswresample/version_major.h)
################################################################################

set(SS_ENABLE_UNIT_TEST OFF CACHE BOOL "enable unit test")
if (SS_ENABLE_UNIT_TEST)
    ################################################################################
    # gtest...
    set(INSTALL_GTEST OFF CACHE INTERNAL "")
    FetchContent_Declare(
        gg_gtest
        URL "${CMAKE_CURRENT_SOURCE_DIR}/3rd/googletest-1.14.0.zip"
    )
    FetchContent_MakeAvailable(gg_gtest)
    set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES FOLDER 3rd)
    ################################################################################

    set(UNIT_TEST_SRC
        ./unit_test/Common.hpp
        ./unit_test/Array_test.cpp
        ./unit_test/StringStream_test.cpp
    )
    add_executable(unit_test ${UNIT_TEST_SRC})
    target_include_directories(unit_test PRIVATE ./src)
    target_link_libraries(unit_test PRIVATE gmock_main)
endif()

##

list(APPEND SS_SRC
    ./src/xm/PlatformDefine.hpp
    ./src/xm/NonCopyable.hpp
    ./src/xm/Array.hpp
    ./src/xm/StringStream.hpp
    ./src/xm/OtherTool.hpp
    ./src/xm/SingletonBase.hpp

    ./src/ss/Pch.hpp
    ./src/ss/BlockingQueue.hpp
    ./src/ss/Common.hpp
    ./src/ss/GlRender.hpp
    ./src/ss/GlRender.cpp
    ./src/ss/MainThread.hpp
    ./src/ss/MainThread.cpp
    ./src/ss/NetThread.hpp
    ./src/ss/NetThread.cpp
    ./src/ss/DecodeThread.hpp
    ./src/ss/DecodeThread.cpp
    ./src/ss/PtsThread.hpp
    ./src/ss/PtsThread.cpp
    ./src/ss/Main.cpp
)

add_executable(share_screen ${SS_EXE_OPTION} ${SS_SRC})
target_compile_definitions(share_screen PRIVATE
    _SS_BUILD_TYPE_NAME=$<CONFIG>
    _SS_VERSION_MAJOR=${SS_VERSION_MAJOR}
    _SS_VERSION_MINOR=${SS_VERSION_MINOR}
    _SS_VERSION_PATCH=${SS_VERSION_PATCH}
    _SS_VERSION_CODE=${SS_VERSION_CODE}
    ${SS_DEFS}
)
target_include_directories(share_screen PRIVATE ./src ./3rd)
target_link_libraries(share_screen PRIVATE uv_a avcodec avutil swresample ${SS_LINK_LIBS})
target_precompile_headers(share_screen PRIVATE ./src/ss/Pch.hpp)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(share_screen PRIVATE "-Wl,--disable-new-dtags")
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SS_SRC})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(TARGET share_screen POST_BUILD COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy.cmake"
        "run.bat"
        "${CMAKE_CURRENT_SOURCE_DIR}/../resource/"
        "$<TARGET_FILE_DIR:share_screen>"
    )
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET share_screen POST_BUILD COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy.cmake"
        "run.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/../resource/"
        "$<TARGET_FILE_DIR:share_screen>"
    )
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
endif()
