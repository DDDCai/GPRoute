cmake_minimum_required(VERSION 3.10)
project(shutdown)

include_directories(.)

aux_source_directory(. CODES)

add_compile_options(
    -O3
    -g
)

if(NODE_NUM)
    add_compile_definitions(NODE_NUM=${NODE_NUM})
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})
add_definitions(${GLIB2_CFLAGS_OTHER})

add_executable(shutdown ${CODES})
target_link_libraries(shutdown ${GLIB2_LIBRARIES} pthread)