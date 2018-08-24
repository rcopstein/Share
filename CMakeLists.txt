cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(Share C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall --pedantic -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_C_STANDARD 99)

find_package(FUSE REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/include")
file(GLOB SOURCES "src/*.c")

add_executable(share src/server.c src/protocols.c src/protocol_join.c src/members.c src/output.c src/nops.c src/fops.c src/nfs_ops_darwin.c)
include_directories(${FUSE_INCLUDE_DIR})
target_link_libraries(share ${FUSE_LIBRARIES})