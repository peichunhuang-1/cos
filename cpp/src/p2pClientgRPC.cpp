#include "cos/p2pClientgRPC.hpp"

namespace cos_core {
    p2pClientgRPC::p2pClientgRPC(const string& name_, const string& source_name_, const string& srv_addr) 
    : name(name_), source_name(source_name_) {
        DLOG(INFO) << "Create client to: " << srv_addr;
        channel_ = CreateChannel(srv_addr, InsecureChannelCredentials());
        stub_ = NodeService::NewStub(channel_);
    }

    void p2pClientgRPC::sayHello() {
        unique_ptr<ClientContext> context_ = make_unique<ClientContext>();
        sayHelloRequest request;
        request.set_node(source_name);
        sayHelloReply reply;
        Status status = stub_->sayHello(context_.get(), request, &reply);
        if (status.ok()) {
            DLOG(INFO) << "Hello reply from: " << reply.node();
        }
    }

    void p2pClientgRPC::advertise(const string& topic, const string& type_url) {
        unique_ptr<ClientContext> context_ = make_unique<ClientContext>();
        PublisherRequest request;
        google::protobuf::Empty res;
        request.set_node(source_name);
        request.set_topic(topic);
        request.set_type_url(type_url);
        Status status = stub_->advertise(context_.get(), request, &res);
        if (status.ok()) {}
        else {}
    }
    void p2pClientgRPC::subscribe(const string& topic, const string& ip, const int& port, const string& type_url) {
        unique_ptr<ClientContext> context_ = make_unique<ClientContext>();
        SubscriberRequest request;
        google::protobuf::Empty res;
        request.set_node(source_name);
        request.set_topic(topic);
        request.set_srv_ip(ip);
        request.set_srv_port(port);
        request.set_type_url(type_url);
        Status status = stub_->subscribe(context_.get(), request, &res);
        if (status.ok()) {}
        else {}
    }
    void p2pClientgRPC::authorize(const string& topic, const string& ip, const int& port, const string& type_url) {
        unique_ptr<ClientContext> context_ = make_unique<ClientContext>();
        AuthorizeRequest request;
        google::protobuf::Empty res;
        request.set_node(source_name);
        request.set_topic(topic);
        request.set_clt_ip(ip);
        request.set_clt_port(port);
        request.set_type_url(type_url);
        Status status = stub_->authorize(context_.get(), request, &res);
        if (status.ok()) {}
        else {}
    }

    p2pClientgRPC::~p2pClientgRPC() {}
}