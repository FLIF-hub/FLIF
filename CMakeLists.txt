cmake_minimum_required(VERSION 2.8.12)
project(flif)

find_package(PNG REQUIRED)

set(IMAGE_SOURCES
    image/color_range.cpp
    image/crc32k.cpp
    image/image-pam.cpp
    image/image-png.cpp
    image/image-pnm.cpp
    image/image.cpp
)

set(MANIAC_SOURCES
    maniac/chance.cpp
)

set(TRANSFORM_SOURCES
    transform/factory.cpp
)

# Use cmake -DCMAKE_CXX_FLAGS=-pg to support other build types.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if(USE_ASAN)
    message(STATUS "Using ASAN")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

set(PROJECT_SOURCES
    ${IMAGE_SOURCES}
    ${MANIAC_SOURCES}
    ${TRANSFORM_SOURCES}
    io.cpp
    common.cpp
    flif-dec.cpp
    flif-enc.cpp
    flif.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME}
    ${PNG_LIBRARY}
)
