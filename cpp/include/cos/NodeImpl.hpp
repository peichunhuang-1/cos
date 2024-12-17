#ifndef NODE_HPP
#define NODE_HPP

#include "cos/Register.hpp"
#include "cos/p2pServergRPC.hpp"
#include "cos/RegistedTopicManager.hpp"

namespace cos_core {
    using namespace std;

    class NodeImpl: public Register, public RegistedTopicManager {
    public:
        NodeImpl(const string& namespace_, const string& name, const string& local_ip = "127.0.0.1", const int ttl = 1);
        ~NodeImpl();
        void                                                            handleEvent();
        void                                                            subscribeImpl(const string& topic, const string& ip, const string& type_url);
        void                                                            advertiseImpl(const string& topic, const string& type_url);
        void                                                            publishImpl(const string& topic, const string& type_url, const string& msg);
        void                                                            spinImpl(const string& topic, const string& type_url, const function<void(const string&)>& cb);
    private:
        const string                                                    create_valid_name(const string& namespace_, const string& name);
        bool                                                            seperate_topic_type(const string& key, string& topic, string& type_url);
        unique_ptr<grpc::Server>                                        p2pServer;
        shared_ptr<p2pServergRPC>                                       p2p_service;
        shared_ptr<uvw::timer_handle>                                   timer;
    };

}

#endif