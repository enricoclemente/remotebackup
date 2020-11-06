cmake_minimum_required(VERSION 3.15)
project(TCPClient)

set(CMAKE_CXX_STANDARD 17)

set(Boost_USE_STATIC_LIBS OFF) #enable dynamic linking
set(Boost_INCLUDE_DIR /Users/enricoclemente/CLionProjects/boost/boost_1_73_0)
set(Boost_LIBRARIES /Users/enricoclemente/CLionProjects/boost/boost_1_73_0/libs)

find_package(Boost REQUIRED COMPONENTS filesystem)

include_directories(${Boost_INCLUDE_DIR})

add_executable(TCPClient main.cpp FileWatcher.cpp FileWatcher.h OutputQueue.cpp OutputQueue.h)

target_link_libraries(TCPClient ${Boost_LIBRARIES})
