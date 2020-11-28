cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)

project(remotebackup)

set(Boost_USE_STATIC_LIBS OFF) #enable dynamic linking

find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(Protobuf REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(rb_lib)

include_directories(${RB_LIB_HEADERS})

add_subdirectory(rb_client)
add_subdirectory(rb_server)
