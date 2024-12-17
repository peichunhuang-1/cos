#ifndef P2P_CLIENT_GRPC_HPP
#define P2P_CLIENT_GRPC_HPP
#include "node.grpc.pb.h"
#include <thread>
#include <shared_mutex>
#include <queue>
#include <grpcpp/grpcpp.h>
#include <glog/logging.h>

namespace cos_core {
    using namespace std;
    using namespace grpc;

    class p2pClientgRPC {
    public:
        p2pClientgRPC(const string& name_, const string& source_name_, const string& srv_addr) ;
        ~p2pClientgRPC();
        void                                sayHello();
        void                                advertise(const string& topic, const string& type_url);
        void                                subscribe(const string& topic, const string& ip, const int& port, const string& type_url);
        void                                authorize(const string& topic, const string& ip, const int& port, const string& type_url);
    private:
        using ClientStub = unique_ptr<NodeService::Stub>;
        shared_ptr<grpc::Channel>           channel_;
        ClientStub                          stub_;
        const string                        name;
        const string                        source_name;
    };
}

#endif