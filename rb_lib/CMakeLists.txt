file(GLOB rb_PROTOS
    "*.proto"
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${rb_PROTOS})

file(GLOB rb_lib_SRC
    "inc/*.h"
    "src/*.cpp"
)

set(RB_LIB_HEADERS ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
link_libraries(${Boost_LIBRARIES} ${Protobuf_LIBRARIES})
add_library(rb_lib ${rb_lib_SRC} ${PROTO_SRCS} ${PROTO_HDRS})
