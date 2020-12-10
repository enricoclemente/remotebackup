file(GLOB rb_server_SRC
    "inc/*.h"
    "src/*.cpp"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${remotebackup_SOURCE_DIR}/rb_server/inc)

add_executable(server ${rb_server_SRC})

target_link_libraries(server rb_lib)
link_directories(${remotebackup_SOURCE_DIR}/rb_lib/src)
