cmake_minimum_required(VERSION 3.15)
project(TCPClient)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS OFF) #enable dynamic linking
set(Boost_INCLUDE_DIR /Users/enricoclemente/CLionProjects/boost/boost_1_73_0)
set(Boost_LIBRARIES /Users/enricoclemente/CLionProjects/boost/boost_1_73_0/libs)

find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(Protobuf REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS packet.proto)
# protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS EXPORT_MACRO DLL_EXPORT sample.proto)
# protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS DESCRIPTORS PROTO_DESCS sample.proto)

add_executable(client main.cpp FileWatcher.cpp FileWatcher.h OutputQueue.cpp OutputQueue.h
                Buffer.cpp Buffer.h AsioAdapting.h ProtobufHelpers.cpp ProtobufHelpers.h ${PROTO_SRCS} ${PROTO_HDRS} )

target_link_libraries(client ${Boost_LIBRARIES} ${Protobuf_LIBRARIES})
