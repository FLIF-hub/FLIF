cmake_minimum_required(VERSION 2.8.12)
project(flif)

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

# find SDL2

find_package(PkgConfig QUIET)
pkg_check_modules(PKG_SDL2 QUIET sdl2)

find_path(SDL2_INCLUDE_DIR
  NAMES SDL.h
  HINTS
    ${PKG_SDL2_INCLUDEDIR}
    ${PKG_SDL2_INCLUDE_DIRS}
  PATH_SUFFIXES SDL2
)

find_library(SDL2_LIBRARY
  NAMES SDL2 SDL2main
  HINTS
    ${PKG_SDL2_LIBDIR}
    ${PKG_SDL2_LIBRARY_DIRS}
  PATH_SUFFIXES x64 x86
)

find_library(SDL2MAIN_LIBRARY
  NAMES SDL2main
  HINTS
    ${PKG_SDL2_LIBDIR}
    ${PKG_SDL2_LIBRARY_DIRS}
  PATH_SUFFIXES x64 x86
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
                                  REQUIRED_VARS SDL2_INCLUDE_DIR SDL2_LIBRARY SDL2MAIN_LIBRARY)

# /find SDL2

set(COMMON_SOURCES
    image/color_range.cpp
    image/crc32k.cpp
    image/image-metadata.cpp
    image/image-pam.cpp
    image/image-png.cpp
    image/image-pnm.cpp
    image/image-rggb.cpp
    image/image.cpp
    maniac/bit.cpp
    maniac/chance.cpp
    maniac/symbol.cpp
    transform/factory.cpp
    io.cpp
    common.cpp
    flif-dec.cpp
    ../extern/lodepng.cpp
)

if(WIN32)
    set(STATIC_LINKED_LIBS ${ZLIB_LIBRARY})
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

if(USE_ASAN)
    message(STATUS "Using ASAN")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

set(DEFINITIONS_FOR_ALL_TARGETS
    LODEPNG_NO_COMPILE_PNG LODEPNG_NO_COMPILE_DISK
)

# executable

if(WIN32)
    set(WINDOWS_EXE_SOURCE
        ../build/MSVC/getopt/getopt.c
    )
endif()

add_executable(flif_exe ${COMMON_SOURCES} ${WINDOWS_EXE_SOURCE} flif-enc.cpp flif.cpp)
target_link_libraries(flif_exe ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})
set_target_properties(flif_exe PROPERTIES OUTPUT_NAME flif)

if(WIN32)
    target_include_directories(flif_exe PRIVATE "../build/MSVC/getopt")
    target_compile_definitions(flif_exe PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} STATIC_GETOPT ) # prevents dllexporting symbols for getopt
endif()

# library

add_library(flif_lib SHARED ${COMMON_SOURCES} flif-enc.cpp library/flif-interface.cpp)
add_library(flif_lib_dec SHARED ${COMMON_SOURCES} library/flif-interface_dec.cpp)

target_link_libraries(flif_lib ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})
target_link_libraries(flif_lib_dec ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})

set_target_properties(flif_lib PROPERTIES OUTPUT_NAME flif)
set_target_properties(flif_lib_dec PROPERTIES OUTPUT_NAME flif_dec)

target_compile_definitions(flif_lib PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} FLIF_BUILD_DLL )
target_compile_definitions(flif_lib_dec PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} FLIF_BUILD_DLL DECODER_ONLY )

target_include_directories(flif_exe PRIVATE "../extern")
target_include_directories(flif_lib PRIVATE "../extern")
target_include_directories(flif_lib_dec PRIVATE "../extern")

# viewflif

if(${SDL2_FOUND})
    add_executable(viewflif viewflif.c)
    set_source_files_properties(viewflif.c PROPERTIES LANGUAGE CXX)
    target_link_libraries(viewflif flif_lib_dec ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY})
    target_include_directories(viewflif PRIVATE "library" ${SDL2_INCLUDE_DIR})
    target_compile_definitions(viewflif PRIVATE FLIF_USE_DLL)
endif()

# test

add_executable(libtest ../tools/test.c)
target_link_libraries(libtest flif_lib)
target_include_directories(libtest PRIVATE "library")
target_compile_definitions(libtest PRIVATE FLIF_USE_DLL)

enable_testing()
add_test(NAME libtest COMMAND libtest dummy.flif)
if(UNIX)
    add_test(NAME roundtrip1 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-roundtrip.sh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/2_webp_ll.png 2_webp_ll.flif decoded_2_webp_ll.png)
    add_test(NAME roundtrip2 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-roundtrip.sh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/kodim01.png kodim01.flif decoded_kodim01.png)
    add_test(NAME roundtrip3 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-roundtrip_anim.sh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/endless_war.gif endless_war.flif)
endif()
