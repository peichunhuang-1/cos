#ifndef P2P_SERVER_GRPC_HPP
#define P2P_SERVER_GRPC_HPP

#include <google/protobuf/message.h>
#include "node.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <glog/logging.h>
#include <google/protobuf/empty.pb.h>
#include "cos/RegistedTopicManager.hpp"
#include <shared_mutex>

namespace cos_core {
    using namespace std;
    using namespace grpc;

    class p2pServergRPC final : public NodeService::Service {
    public:
        p2pServergRPC(const string& name_) ;
        Status                                          sayHello(ServerContext* context, 
                                                        const sayHelloRequest*request, sayHelloReply* reply) override;
        Status                                          advertise(ServerContext* context, 
                                                        const PublisherRequest*request, google::protobuf::Empty* reply) override;
        Status                                          subscribe(ServerContext* context, 
                                                        const SubscriberRequest*request, google::protobuf::Empty* reply) override;
        Status                                          authorize(ServerContext* context, 
                                                        const AuthorizeRequest*request, google::protobuf::Empty* reply) override;
    private:
        const string                                    name;
    };

}

#endif