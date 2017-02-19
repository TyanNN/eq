cmake_minimum_required(VERSION 2.8.9)
project(eq)
set(CMAKE_C_COMPILER clang)

file(GLOB eq_SRC
    "src/*.c"
    "lib/*.c"
)

add_executable(eq ${eq_SRC})

include_directories(include lib)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories (${GLIB_INCLUDE_DIRS})
link_directories (${GLIB_LIBRARY_DIRS})
add_definitions (${GLIB_CFLAGS_OTHER})
target_link_libraries (eq ${GLIB_LIBRARIES})

pkg_check_modules(XML REQUIRED libxml-2.0)
include_directories (${XML_INCLUDE_DIRS})
link_directories (${XML_LIBRARY_DIRS})
add_definitions (${XML_CFLAGS_OTHER})
target_link_libraries (eq ${XML_LIBRARIES})
