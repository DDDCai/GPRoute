cmake_minimum_required(VERSION 3.10.2)
project(server)

include_directories(.)

aux_source_directory(. CODES)

add_compile_options(
    -O3
    # -g
)

if(FEATURE_NUM)
    add_compile_definitions(FEATURE_NUM=${FEATURE_NUM})
endif()
if(ROUTE_METHOD)
    add_compile_definitions(ROUTE_METHOD=${ROUTE_METHOD})
endif()
if(GP_SIZE)
    add_compile_definitions(GP_SIZE=${GP_SIZE})
endif()
if(HIT_THRES)
    add_compile_definitions(HIT_THRES=${HIT_THRES})
endif()
if(MAX_THREAD_POOL_SIZE)
    add_compile_definitions(MAX_THREAD_POOL_SIZE=${MAX_THREAD_POOL_SIZE})
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED glib-2.0 gio-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})
add_definitions(${GLIB2_CFLAGS_OTHER})

add_executable(server ${CODES})
target_link_libraries(server ${GLIB2_LIBRARIES} pthread)