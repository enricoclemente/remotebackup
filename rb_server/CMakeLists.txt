file(GLOB rb_server_SRC
    "inc/*.h"
    "src/*.cpp"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${remotebackup_SOURCE_DIR}/rb_server/inc)

add_executable(server ${rb_server_SRC})

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(server rb_lib ${SQLite3_LIBRARIES} OpenSSL::Crypto)
link_directories(${remotebackup_SOURCE_DIR}/rb_lib/src)
