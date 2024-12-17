add_executable(NodeImpl Register.cpp p2pClientgRPC.cpp NodeImpl.cpp p2pServergRPC.cpp p2p_event.cpp RegistedTopicManager.cpp)
target_link_libraries(NodeImpl PRIVATE etcd-cpp-api
                               ${LIBUV_LIBRARIES}
                               PUBLIC glog::glog 
                               ${_REFLECTION} 
                               ${_GRPC_GRPCPP} 
                               ${_PROTOBUF_LIBPROTOBUF} 
                               node_grpc_proto 
                               )