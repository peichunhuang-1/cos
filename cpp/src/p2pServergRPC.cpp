#include "cos/p2pServergRPC.hpp"
#include "cos/p2p_event.hpp"
namespace cos_core {
    p2pServergRPC::p2pServergRPC(const string& name_) : name(name_) {}

    Status p2pServergRPC::sayHello(ServerContext* context, const sayHelloRequest*request, sayHelloReply* reply) {
        DLOG(INFO) << "Hello from: " << request->node();
        reply->set_node(name);
        return Status::OK;
    }

    Status p2pServergRPC::advertise(ServerContext* context, 
    const PublisherRequest*request, google::protobuf::Empty* reply) {
        p2pEventPools::pushEvent(name, p2pEvent::p2pAddRemotePublisher(request->node(), request->topic(), request->type_url()));
        return Status::OK;
    }
    Status p2pServergRPC::subscribe(ServerContext* context, 
    const SubscriberRequest*request, google::protobuf::Empty* reply) {
        p2pEventPools::pushEvent(name, p2pEvent::p2pAddRemoteSubscriber(request->node(), request->topic(), request->srv_ip(), request->srv_port(), request->type_url()));
        return Status::OK;
    }
    Status p2pServergRPC::authorize(ServerContext* context, 
    const AuthorizeRequest*request, google::protobuf::Empty* reply) {
        p2pEventPools::pushEvent(name, p2pEvent::p2pAuthorizeIO(request->node(), request->topic(), request->clt_ip(), request->clt_port(), request->type_url()));
        return Status::OK;
    }

}