file(GLOB rb_client_SRC
        "inc/*.h"
        "src/*.cpp"
        )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${remotebackup_SOURCE_DIR}/rb_client/inc)

add_executable(client ${rb_client_SRC})

target_link_libraries(client rb_lib)
link_directories(${remotebackup_SOURCE_DIR}/rb_lib/src)
