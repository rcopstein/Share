cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(Share LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall --pedantic -g")
if (UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -lm")
endif(UNIX AND NOT APPLE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_C_STANDARD 99)

find_package(FUSE REQUIRED)
find_package(Threads REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/include")
file(GLOB GLOBAL_SOURCES "src/*.c")

if (APPLE)
    file(GLOB SPECIFIC_SOURCES "src/Darwin/*.c")
elseif(UNIX)
    file(GLOB SPECIFIC_SOURCES "src/Unix/*.c")
endif(APPLE)

set(SOURCES ${GLOBAL_SOURCES} ${SPECIFIC_SOURCES})

include_directories(${FUSE_INCLUDE_DIR})

add_executable(share ${SOURCES})
target_link_libraries(share ${FUSE_LIBRARIES})

if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(share PUBLIC "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(share "${CMAKE_THREAD_LIBS_INIT}")
endif()