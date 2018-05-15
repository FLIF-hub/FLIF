cmake_minimum_required(VERSION 2.8.12)
project(flif)
 
include(GNUInstallDirs)
include(FindPkgConfig)
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})
option(BUILD_SHARED_LIBS "Build shared FLIF encoder/decoder libraries" ON)
option(BUILD_STATIC_LIBS "Build static FLIF encoder/decoder libraries" ON)

set(SOVERSION 0)
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

if(PKG_CONFIG_FOUND)
pkg_check_modules(GDK gdk-pixbuf-2.0 )
endif(PKG_CONFIG_FOUND)
set(FLIF_SRC_DIR ${CMAKE_CURRENT_LIST_DIR})

set(COMMON_SOURCES
    ${FLIF_SRC_DIR}/image/color_range.cpp
    ${FLIF_SRC_DIR}/image/crc32k.cpp
    ${FLIF_SRC_DIR}/image/image-metadata.cpp
    ${FLIF_SRC_DIR}/image/image-pam.cpp
    ${FLIF_SRC_DIR}/image/image-png.cpp
    ${FLIF_SRC_DIR}/image/image-pnm.cpp
    ${FLIF_SRC_DIR}/image/image-rggb.cpp
    ${FLIF_SRC_DIR}/image/image.cpp
    ${FLIF_SRC_DIR}/maniac/bit.cpp
    ${FLIF_SRC_DIR}/maniac/chance.cpp
    ${FLIF_SRC_DIR}/maniac/symbol.cpp
    ${FLIF_SRC_DIR}/transform/factory.cpp
    ${FLIF_SRC_DIR}/io.cpp
    ${FLIF_SRC_DIR}/common.cpp
    ${FLIF_SRC_DIR}/flif-dec.cpp
    ${FLIF_SRC_DIR}/../extern/lodepng.cpp
)

set(FLIF_DEC_FILES
  ${FLIF_SRC_DIR}/library/flif_dec.h 
  ${FLIF_SRC_DIR}/library/flif-interface-private_dec.hpp
  ${FLIF_SRC_DIR}/library/flif-interface_dec.cpp)

set(FLIF_DEC_HEADERS
    ${FLIF_SRC_DIR}/library/flif_common.h
    ${FLIF_SRC_DIR}/library/flif_dec.h
)
set(FLIF_ENC_FILES
  ${FLIF_SRC_DIR}/flif-enc.cpp
  ${FLIF_SRC_DIR}/library/flif-interface.cpp
)
set(FLIF_ENC_HEADERS
    ${FLIF_SRC_DIR}/library/flif.h
    ${FLIF_SRC_DIR}/library/flif_common.h
    ${FLIF_SRC_DIR}/library/flif_dec.h
    ${FLIF_SRC_DIR}/library/flif_enc.h
)

if(WIN32)
    set(STATIC_LINKED_LIBS ${ZLIB_LIBRARY})
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    if(WIN32)
#This is neeeded to prevent warnings such as:
#mingw The ABI for passing parameters with 16-byte alignment has changed in GCC 4.6
#if you compile for i686.
#but the downside is that the compiled binaries might not work with some old CPU's.
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx")
    endif(WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ftree-vectorize")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -ftree-vectorize")
else()
# Clang specific things
# from: https://stackoverflow.com/questions/10046114/in-cmake-how-can-i-test-if-the-compiler-is-clang
    if (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]") #Case insensitive match
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
       set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
       if(WIN32)
#This is neeeded to prevent warnings such as:
#mingw The ABI for passing parameters with 16-byte alignment has changed in GCC 4.6
#if you compile for i686.
#but the downside is that the compiled binaries might not work with some old CPU's.
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
          set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx")
      endif(WIN32)
      set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
      set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
      set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ftree-vectorize")
      set(CMAKE_C_FLAGS_RELEASE "-O3 -ftree-vectorize")
    endif(CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]") #Case insensitive match
endif(CMAKE_COMPILER_IS_GNUCXX)

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
        ${FLIF_SRC_DIR}/../build/MSVC/getopt/getopt.c
    )
endif()

add_executable(flif_exe ${COMMON_SOURCES} ${WINDOWS_EXE_SOURCE} ${FLIF_SRC_DIR}/flif-enc.cpp ${FLIF_SRC_DIR}/flif.cpp)
target_link_libraries(flif_exe ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})
set_target_properties(flif_exe PROPERTIES OUTPUT_NAME flif)
add_executable(dflif_exe ${COMMON_SOURCES} ${WINDOWS_EXE_SOURCE} ${FLIF_SRC_DIR}/flif-enc.cpp ${FLIF_SRC_DIR}/flif.cpp)
target_compile_definitions(dflif_exe PRIVATE DECODER_ONLY)
target_link_libraries(dflif_exe ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})
set_target_properties(dflif_exe PROPERTIES OUTPUT_NAME dflif)

if(WIN32)
    target_include_directories(flif_exe PRIVATE ${FLIF_SRC_DIR}/../build/MSVC/getopt)
    target_compile_definitions(flif_exe PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} STATIC_GETOPT ) # prevents dllexporting symbols for getopt
    target_include_directories(dflif_exe PRIVATE ${FLIF_SRC_DIR}/../build/MSVC/getopt)
    target_compile_definitions(dflif_exe PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} STATIC_GETOPT ) # prevents dllexporting symbols for getopt
endif()

# library
if(BUILD_SHARED_LIBS)
    add_library(flif_lib SHARED ${COMMON_SOURCES} ${FLIF_ENC_FILES})
    add_library(flif_lib_dec SHARED ${COMMON_SOURCES} ${FLIF_DEC_FILES} ${FLIF_DEC_HEADERS})

    target_link_libraries(flif_lib ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})
    target_link_libraries(flif_lib_dec ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})

    set_target_properties(flif_lib PROPERTIES OUTPUT_NAME flif)
    set_target_properties(flif_lib_dec PROPERTIES OUTPUT_NAME flif_dec)
  
    set_target_properties(flif_lib PROPERTIES PUBLIC_HEADER "${FLIF_ENC_HEADERS}" SOVERSION ${SOVERSION})   
    set_target_properties(flif_lib_dec PROPERTIES PUBLIC_HEADER "${FLIF_DEC_HEADERS}" SOVERSION ${SOVERSION})

    target_compile_definitions(flif_lib PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} FLIF_BUILD_DLL )
    target_compile_definitions(flif_lib_dec PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} FLIF_BUILD_DLL DECODER_ONLY )

    target_include_directories(flif_lib PRIVATE ${FLIF_SRC_DIR}/../extern)
    target_include_directories(flif_lib_dec PRIVATE ${FLIF_SRC_DIR}/../extern)

    target_link_libraries(flif_exe flif_lib)
    target_link_libraries(dflif_exe flif_lib_dec)
    install(TARGETS flif_lib flif_lib_dec
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      PUBLIC_HEADER DESTINATION include)
endif(BUILD_SHARED_LIBS)

target_include_directories(flif_exe PRIVATE ${FLIF_SRC_DIR}/../extern)
target_include_directories(dflif_exe PRIVATE ${FLIF_SRC_DIR}/../extern)
install(TARGETS flif_exe dflif_exe
    RUNTIME DESTINATION bin)
install(FILES ${FLIF_SRC_DIR}/../tools/gif2flif 
   PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_READ DESTINATION bin)
install(FILES ${FLIF_SRC_DIR}/../tools/apng2flif 
   PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_READ DESTINATION bin)
install(FILES ${FLIF_SRC_DIR}/../doc/flif.1
   DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1)


if(BUILD_STATIC_LIBS)
    add_library(flif_lib_static STATIC ${COMMON_SOURCES} ${FLIF_ENC_FILES})
    add_library(flif_lib_dec_static STATIC ${COMMON_SOURCES} ${FLIF_DEC_FILES})

    target_link_libraries(flif_lib_static ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})
    target_link_libraries(flif_lib_dec_static ${PNG_LIBRARY} ${STATIC_LINKED_LIBS})

    set_target_properties(flif_lib_static PROPERTIES OUTPUT_NAME flif)
    set_target_properties(flif_lib_dec_static PROPERTIES OUTPUT_NAME flif_dec)

    set_target_properties(flif_lib_static PROPERTIES PUBLIC_HEADER "${FLIF_ENC_HEADERS}" SOVERSION ${SOVERSION})   
    set_target_properties(flif_lib_dec_static PROPERTIES PUBLIC_HEADER "${FLIF_DEC_HEADERS}" SOVERSION ${SOVERSION})

    target_compile_definitions(flif_lib_static PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS})
    target_compile_definitions(flif_lib_dec_static PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS} DECODER_ONLY )

    target_include_directories(flif_lib_static PRIVATE ${FLIF_SRC_DIR}/../extern)
    target_include_directories(flif_lib_dec_static PRIVATE ${FLIF_SRC_DIR}/../extern)

    install(TARGETS flif_lib_static flif_lib_dec_static
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      PUBLIC_HEADER DESTINATION include)

    if (NOT(BUILD_SHARED_LIBS))
       target_link_libraries(flif_exe flif_lib_static)
       target_link_libraries(dflif_exe flif_lib_dec_static)
    endif(NOT(BUILD_SHARED_LIBS))
endif(BUILD_STATIC_LIBS)

# gdk-pixbuf loader, enabling FLIF-viewing in pixbuf applications like Eye of Gnome
if(GDK_FOUND AND (BUILD_SHARED_LIBS OR BUILD_STATIC_LIBS))
## The loader is LGPLed so there's no reason to use libflif_dec here
## -Werror=implicit-function-declaration is useful here to check for preprocessor dependency errors
#libpixbufloader-flif$(LIBEXT): libflif$(LIBEXT) flif-pixbuf-loader.c
#	$(CC) -shared $(CFLAGS) -Ilibrary/ -fPIC -Wall -Werror=implicit-function-declaration $(shell pkg-config --cflags gdk-pixbuf-2.0) -o libpixbufloader-flif$(LIBEXT) flif-pixbuf-loader.c $(LDFLAGS) $(shell pkg-config --libs gdk-pixbuf-2.0) -L. -lflif
    add_library(pixbufloader-flif SHARED flif-pixbuf-loader.c)
    target_include_directories(pixbufloader-flif PRIVATE ${FLIF_SRC_DIR}/library ${GDK_INCLUDE_DIRS})
    target_compile_options(pixbufloader-flif PRIVATE ${GDK_CFLAGS})
    target_compile_definitions(pixbufloader-flif PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS})
    target_link_libraries(pixbufloader-flif ${GDK_LIBRARIES})
    set_target_properties(pixbufloader-flif PROPERTIES OUTPUT_NAME pixbufloader-flif)

    add_library(pixbufloader-flif-static STATIC flif-pixbuf-loader.c)
    target_include_directories(pixbufloader-flif-static PRIVATE ${FLIF_SRC_DIR}/library ${GDK_INCLUDE_DIRS})
    target_compile_options(pixbufloader-flif-static PRIVATE ${GDK_CFLAGS})
    target_compile_definitions(pixbufloader-flif-static PRIVATE ${DEFINITIONS_FOR_ALL_TARGETS})
    target_link_libraries(pixbufloader-flif-static ${GDK_LIBRARIES})
    set_target_properties(pixbufloader-flif-static PROPERTIES OUTPUT_NAME pixbufloader-flif)
    if (BUILD_SHARED_LIBS)
       target_link_libraries(pixbufloader-flif flif_lib)
       target_link_libraries(pixbufloader-flif-static flif_lib) 
    else()
       target_link_libraries(pixbufloader-flif flif_lib_static)
       target_link_libraries(pixbufloader-flif-static flif_lib_static)
    endif(BUILD_SHARED_LIBS)
#We have to be sure that the "-dont-define-prefix", otherwise, on mingw-w64, a complete path
#is given with pkg-config and that's not desirable for some packaging systems.
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=gdk_pixbuf_moduledir --dont-define-prefix gdk-pixbuf-2.0
       OUTPUT_VARIABLE GDK_MODULES_DIR ERROR_VARIABLE GDK_RES_ERROR RESULT_VARIABLE RES_VAR OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (("${RES_VAR}" STREQUAL "") OR ("${RES_VAR}" STREQUAL "0"))
           message(STATUS "gdk-pixbuf-2.0 module dir ${GDK_MODULES_DIR}")      
    else()
           message(FATAL_ERROR "Error: ${RES_VAR}")
    endif(("${RES_VAR}" STREQUAL "") OR ("${RES_VAR}" STREQUAL "0"))
    if ("${GDK_RES_ERROR}" STREQUAL "")
    install(TARGETS pixbufloader-flif pixbufloader-flif-static 
      RUNTIME DESTINATION ${GDK_MODULES_DIR}
      LIBRARY DESTINATION ${GDK_MODULES_DIR}
      ARCHIVE DESTINATION ${GDK_MODULES_DIR}) 
    else()
      message(FATAL_ERROR "Error: ${GDK_RES_ERROR}")
    endif("${GDK_RES_ERROR}" STREQUAL "")
#You would want to do this on a user's system.
#
#    gdk-pixbuf-query-loaders --update-cache
#
# We don't do that here because I see this CMakeLists.txt file for software
#packagers.
endif(GDK_FOUND AND (BUILD_SHARED_LIBS OR BUILD_STATIC_LIBS))

# mime-info
find_program(SHARED_MIME_PROG "update-mime-database")
if (SHARED_MIME_PROG)
    install(FILES ${FLIF_SRC_DIR}/flif-mime.xml
      DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/mime/packages)
#you would want to do something like:
#
#    execute_process(COMMAND #{SHARED_MIME_PROG} ${CMAKE_INSTALL_PREFIX}/share/mime    )
#
#but that type of thing should be done on the user's sytem, not on a build system
#It does require the "shared-mime-info" package
endif(SHARED_MIME_PROG)

# Magic data file
#
# For deployment, you want to do  something like this on the end-user's system
#
#if ! grep -q FLIF /etc/magic; then cat ${CMAKE_INSTALL_FULL_DATAROOTDIR}/doc/flif.magic >> /etc/magic; fi
#
install(FILES ${FLIF_SRC_DIR}/../doc/flif.magic
   DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/FLIF)

# viewflif

if(${SDL2_FOUND} AND ${BUILD_SHARED_LIBS})
    if(WIN32)
      add_executable(viewflif WIN32 ${FLIF_SRC_DIR}/viewflif.c)
    else(WIN32)
      add_executable(viewflif ${FLIF_SRC_DIR}/viewflif.c)
    endif(WIN32)
    if ("${PKG_SDL2_STATIC_LIBRARIES}" STREQUAL "")
      target_link_libraries(viewflif flif_lib_dec ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY})
    else("${PKG_SDL2_STATIC_LIBRARIES}" STREQUAL "")
      target_link_libraries(viewflif flif_lib_dec ${PKG_SDL2_STATIC_LIBRARIES})
    endif("${PKG_SDL2_STATIC_LIBRARIES}" STREQUAL "")
    target_include_directories(viewflif PRIVATE ${FLIF_SRC_DIR}/library ${SDL2_INCLUDE_DIR})
    target_compile_definitions(viewflif PRIVATE FLIF_USE_DLL)
    install(TARGETS viewflif
      RUNTIME DESTINATION bin)
endif(${SDL2_FOUND} AND ${BUILD_SHARED_LIBS})

# test

if(BUILD_SHARED_LIBS)
add_executable(libtest ${FLIF_SRC_DIR}/../tools/test.c)
target_link_libraries(libtest flif_lib)
target_include_directories(libtest PRIVATE ${FLIF_SRC_DIR}/library)
target_compile_definitions(libtest PRIVATE FLIF_USE_DLL)
endif(BUILD_SHARED_LIBS)

if(BUILD_STATIC_LIBS)
add_executable(libtest_static ${FLIF_SRC_DIR}/../tools/test.c)
target_link_libraries(libtest_static flif_lib)
target_include_directories(libtest_static PRIVATE ${FLIF_SRC_DIR}/library)
endif(BUILD_STATIC_LIBS)

# license stuff
install(FILES "${FLIF_SRC_DIR}/../LICENSE" "${FLIF_SRC_DIR}/../LICENSE_Apache2" "${FLIF_SRC_DIR}/../LICENSE_GPL" "${FLIF_SRC_DIR}/../LICENSE_LGPL" "${FLIF_SRC_DIR}/../FLIF-CLA-template.txt"
   DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/FLIF")

enable_testing()
add_test(NAME libtest COMMAND libtest dummy.flif)
add_test(NAME libtest_static COMMAND libtest_static dummy.flif)

if(UNIX)
    add_test(NAME roundtrip1 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-roundtrip.sh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/2_webp_ll.png 2_webp_ll.flif decoded_2_webp_ll.png)
    add_test(NAME roundtrip2 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-roundtrip.sh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/kodim01.png kodim01.flif decoded_kodim01.png)
    add_test(NAME roundtrip3 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-roundtrip_anim.sh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/endless_war.gif endless_war.flif)
endif()
