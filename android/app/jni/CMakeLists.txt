# Native part of the Android test APK (stage 2-B): libSDL2.so, libSDL2_image.so and libmain.so
# (android_main.cpp + the OS 5 core), loaded in that order by EmulatorActivity (SDLActivity).
# SDL and SDL_image sources are fetched by tools/android-fetch-deps.sh into android/deps at pinned commits.
cmake_minimum_required(VERSION 3.22)
project(cloudpilot_android C CXX)

get_filename_component(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)
set(DEPS ${REPO}/android/deps)

# 16 KiB page size: ELF LOAD segments aligned to 16 KiB in every shared library.
add_link_options(-Wl,-z,max-page-size=16384)

# SDL2: shared only, no test library.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL SDL)

# SDL2_image: shared (a static build needs a static SDL2), built-in loaders only (PNG through
# stb_image, PNG save through miniz), no external image libraries.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
foreach(format AVIF JXL TIF WEBP)
    set(SDL2IMAGE_${format} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory(${DEPS}/SDL_image SDL_image)

# Core sources: the same SOURCE_C/SOURCE_CXX lists the native and web builds use.
function(make_list out makefile var)
    file(READ ${makefile} text)
    string(REGEX REPLACE "\\\\\n" " " text "${text}")  # join continuation lines
    string(REGEX MATCH "\n${var} *=[^\n]*" match "\n${text}")
    string(REGEX REPLACE "^\n${var} *=" "" match "${match}")
    string(REGEX REPLACE "[ \t\n]+" ";" match "${match}")
    set(result "")
    foreach(item ${match})
        if(NOT item MATCHES "^\\$\\(")
            list(APPEND result ${item})
        endif()
    endforeach()
    set(${out} ${result} PARENT_SCOPE)
endfunction()

set(CORE_SOURCES "")
foreach(dir uarm common)
    foreach(var SOURCE_C SOURCE_CXX)
        make_list(files ${REPO}/src/${dir}/Makefile ${var})
        list(TRANSFORM files PREPEND ${REPO}/src/${dir}/)
        if(dir STREQUAL "common" AND var STREQUAL "SOURCE_C")
            set_source_files_properties(${files} PROPERTIES COMPILE_OPTIONS "-std=gnu99")
        endif()
        list(APPEND CORE_SOURCES ${files})
    endforeach()
endforeach()
list(LENGTH CORE_SOURCES core_count)
message(STATUS "OS 5 core: ${core_count} source files")

set(CORE_INCLUDES
    ${REPO}/src/uarm/uarm ${REPO}/src/uarm ${REPO}/src/common ${REPO}/src/common/zip
    ${REPO}/src/argparse ${REPO}/src/cloudpilot/nanopb ${REPO}/src/cloudpilot/proto)

# Same warning flags as src/uarm/Makefile; -O2 also in debug APKs (emulation speed).
add_library(uarmcore OBJECT ${CORE_SOURCES})
set_target_properties(uarmcore PROPERTIES C_STANDARD 11 C_EXTENSIONS ON CXX_STANDARD 17
                                          POSITION_INDEPENDENT_CODE ON)
target_include_directories(uarmcore PRIVATE ${CORE_INCLUDES})
target_compile_options(uarmcore PRIVATE
    -O2 -Wextra -Wall -Wno-unused-parameter -Wno-unknown-pragmas -Wno-visibility
    -Wno-constant-conversion -Wno-constant-logical-operand)
target_compile_definitions(uarmcore PRIVATE _FILE_OFFSET_BITS=64)

# libmain.so: every core object is linked (object library, not an archive), plus the SDL
# frontend files of the native build (SOURCE_CXX_NATIVE in src/uarm/Makefile) that need neither
# the console, the debugger nor NetLib.
add_library(main SHARED
    ${REPO}/src/uarm/native/android_main.cpp
    ${REPO}/src/uarm/native/SdlRenderer.cpp
    ${REPO}/src/uarm/native/SdlEventHandler.cpp
    ${REPO}/src/uarm/native/SdlAudioDriver.cpp
    ${REPO}/src/uarm/native/Silkscreen.cpp)
set_target_properties(main PROPERTIES CXX_STANDARD 17)
target_include_directories(main PRIVATE ${CORE_INCLUDES})
target_compile_options(main PRIVATE -Wextra -Wall -Wno-unused-parameter)
target_link_libraries(main PRIVATE uarmcore SDL2::SDL2 SDL2_image)
if(ANDROID)
    target_link_libraries(main PRIVATE log)
endif()
target_link_options(main PRIVATE -Wl,--no-undefined)
