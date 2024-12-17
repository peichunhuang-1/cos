cmake_minimum_required(VERSION 3.8)
project(cos C CXX)

set (CMAKE_EXE_LINKER_FLAGS)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -fPIC")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories(${CMAKE_PREFIX_PATH}/include)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(etcd-cpp-api 0.15.4 REQUIRED)
find_package (glog 0.6.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)


add_executable(test test.cpp)
target_link_libraries(test PRIVATE etcd-cpp-api)

add_subdirectory(cpp)

macro(target_public_headers TARGET)
  set_target_properties(${TARGET} PROPERTIES PUBLIC_HEADER "${ARGN}")
endmacro()

set(PROTONAMES 
    node
)
set(PROTO_PATH ${CMAKE_CURRENT_LIST_DIR}/protos)

foreach(PROTONAME ${PROTONAMES})
    # Add command to generate c/c++ code
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.cc"
      "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.h"
      "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.grpc.pb.cc" 
      "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.grpc.pb.h"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        # --js_out=import_style=commonjs:"${CMAKE_CURRENT_BINARY_DIR}"
        # --grpc-web_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${CMAKE_CURRENT_LIST_DIR}/protos"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        # --plugin=protoc-gen-grpc-web="${_GRPC_NODE_PLUGIN_EXECUTABLE}"
        "${PROTO_PATH}/${PROTONAME}.proto"
      DEPENDS "${PROTO_PATH}/${PROTONAME}.proto")

    add_library("${PROTONAME}_grpc_proto"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.h"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.grpc.pb.cc" 
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.grpc.pb.h")
    target_link_libraries("${PROTONAME}_grpc_proto"
      ${_REFLECTION}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF})

    add_library("${PROTONAME}_proto"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.h")
    target_link_libraries("${PROTONAME}_proto"
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
    
    target_public_headers(
    "${PROTONAME}_grpc_proto" 
    "${PROTO_PATH}/${PROTONAME}.proto" 
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.grpc.pb.h"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROTONAME}.pb.h")

    INSTALL(TARGETS "${PROTONAME}_grpc_proto" "${PROTONAME}_proto"
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endforeach()