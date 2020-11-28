protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS packet.proto)

file(GLOB rb_lib_SRC
    "inc/*.h"
    "src/*.cpp"
)

include_directories(${remotebackup_SOURCE_DIR}/rb_lib/inc)

set(RB_LIB_HEADERS ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)

add_library(rb_lib ${rb_lib_SRC} ${PROTO_SRCS} ${PROTO_HDRS})
