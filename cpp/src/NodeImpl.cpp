#include "cos/NodeImpl.hpp"
#include <string>
#include <algorithm>
#include <cctype>
#include "cos/p2pServergRPC.hpp"
#include "cos/p2pClientgRPC.hpp"

namespace cos_core {
    NodeImpl::NodeImpl(const string& namespace_, const string& name, const string& local_ip, const int ttl): 
    Register(create_valid_name(namespace_, name), ttl) {
        p2p_service = make_shared<p2pServergRPC>(node_name);
        grpc::ServerBuilder builder;
        int rpc_port_;
        builder.AddListeningPort(local_ip+":0", grpc::InsecureServerCredentials(), &rpc_port_);
        builder.RegisterService(p2p_service.get());
        p2pServer = builder.BuildAndStart();
        DLOG(INFO) << "Start peer to peer server on: " << local_ip + ":" + to_string(rpc_port_);
        startRegist(local_ip + ":" + to_string(rpc_port_));
        timer = uvw::loop::get_default()->resource<uvw::timer_handle>();
        timer->on<uvw::timer_event>([this](uvw::timer_event&, uvw::timer_handle&) {
            this->handleEvent();
        });
        timer->start(uvw::timer_handle::time{0}, uvw::timer_handle::time{200});
    }

    const string NodeImpl::create_valid_name(const string& namespace_, const string& name) {
        /*
        ensure the format: 
        1. start with /
        2. allow upper/lower case character and /
        3. remove double/triple... slash
        4. end with upper/lower case character
        */ 
        string combined = "/" + namespace_ + "/" + name;
        combined.erase(remove_if(combined.begin(), combined.end(),
            [](char c) { return !isalnum(c) && c != '/'; }),
        combined.end());
        string result;
        bool lastWasSlash = false;
        for (char c : combined) {
            if (c == '/') {
                if (!lastWasSlash) {
                    result += c;
                    lastWasSlash = true;
                }
            } else {
                result += c;
                lastWasSlash = false;
            }
        }
        if (result.empty() || result[0] != '/') {
            result = "/" + result;
        }
        if (!result.empty() && result.back() == '/') {
            result.pop_back();
        }
        return result;
    }

    bool NodeImpl::seperate_topic_type(const string& key, string& topic, string& type_url) {
        size_t delimiterPos = key.find('?');
        if (delimiterPos != string::npos) {
            topic = key.substr(0, delimiterPos);
            type_url = key.substr(delimiterPos + 1);

            size_t specialCharPos = type_url.find_first_of("><");
            if (specialCharPos != string::npos) {
                type_url = type_url.substr(0, specialCharPos);
            }
            return true;
        } else {
            return false;
        }
    }
 
    void NodeImpl::handleEvent() {
        p2pEvent event = p2pEvent::empty();
        while (p2pEventPools::popEvent(node_name, event)) {
            switch (event.getType()) {
            case p2pEventType::REGISTRATION: {
                auto peer = handleRegistrationEvent(::get<p2pRegistrationEvent>(event.event));
                if (peer) {
                    peer->sayHello();
                    queue<string> published_topics, subscribed_topics;
                    listAllLocalTopic(published_topics, subscribed_topics);
                    while (!published_topics.empty()) {
                        string topic, type_url;
                        seperate_topic_type(published_topics.front(), topic, type_url);
                        peer->advertise(topic, type_url);
                        published_topics.pop();
                    }
                    while (!subscribed_topics.empty()) {
                        uv_io_t io;
                        string topic, type_url;
                        getRegistedTopicByKey(subscribed_topics.front() + "<*", io);
                        seperate_topic_type(subscribed_topics.front(), topic, type_url);
                        peer->subscribe(topic, io.ip, io.port, type_url);
                        subscribed_topics.pop();
                    }
                }
                break;
            }
            case p2pEventType::ADD_REMOTE_SUBSCRIBER: {
                auto retrieved_event = ::get<p2pAddRemoteSubscriberEvent>(event.event);
                if (insertRemoteSubscribedTopic(retrieved_event.remote_name, retrieved_event.topic, retrieved_event.type_url, 
                retrieved_event.remote_ip, retrieved_event.remote_port) ) {
                    string local_ip; int local_port;
                    modifyLocalPublishedTopic(node_name, retrieved_event.remote_name, retrieved_event.topic, retrieved_event.type_url, 
                    local_ip, local_port);
                    auto client = getRemoteClientByName(retrieved_event.remote_name);
                    if (client) client->authorize(retrieved_event.topic, local_ip, local_port, retrieved_event.type_url);
                }
                break;
            }
            case p2pEventType::ADD_REMOTE_PUBLISHER: {
                auto retrieved_event = ::get<p2pAddRemotePublisherEvent>(event.event);
                string local_ip; int local_port;
                if (insertRemotePublishedTopic(retrieved_event.remote_name, retrieved_event.topic, retrieved_event.type_url, 
                local_ip, local_port) ) {
                    auto client = getRemoteClientByName(retrieved_event.remote_name);
                    if (client) client->subscribe(retrieved_event.topic, local_ip, local_port, retrieved_event.type_url);
                }
                break;
            }
            case p2pEventType::ADD_LOCAL_SUBSCRIBER: {
                auto retrieved_event = ::get<p2pAddLocalSubscriberEvent>(event.event);
                int port;
                insertLocalSubscribedTopic(retrieved_event.topic, retrieved_event.type_url, retrieved_event.local_ip, port);
                auto clients = listAllExistClients();
                for (auto client: clients) {
                    client->subscribe(retrieved_event.topic, retrieved_event.local_ip, port, retrieved_event.type_url);
                }
                break;
            }
            case p2pEventType::ADD_LOCAL_PUBLISHER: {
                auto retrieved_event = ::get<p2pAddLocalPublisherEvent>(event.event);
                insertLocalPublishedTopic(retrieved_event.topic, retrieved_event.type_url);
                auto clients = listAllExistClients();
                for (auto client: clients) {
                    client->advertise(retrieved_event.topic, retrieved_event.type_url);
                }
                break;
            }
            case p2pEventType::AUTHORIZE_IO: {
                auto retrieved_event = ::get<p2pAuthorizeIOEvent>(event.event);
                modifyLocalSubscribedTopic(retrieved_event.remote_name, retrieved_event.topic, retrieved_event.type_url, 
                retrieved_event.ip, retrieved_event.port);
                break;
            }
            default:
                break;
            }
        }
    }

    void NodeImpl::subscribeImpl(const string& topic, const string& ip, const string& type_url) {
        p2pEventPools::pushEvent(node_name, p2pEvent::p2pAddLocalSubscriber(topic, ip, type_url));
    }

    void NodeImpl::advertiseImpl(const string& topic, const string& type_url) {
        p2pEventPools::pushEvent(node_name, p2pEvent::p2pAddLocalPublisher(topic, type_url));
    }

    void NodeImpl::publishImpl(const string& topic, const string& type_url, const string& msg) {
        auto io_queue = getRegistedTopicByPrefix(topic + "?" + type_url + ">/");
        while (!io_queue.empty()) {
            io_queue.front().buf->enqueue(msg);
            io_queue.front().trigger->send();
            io_queue.pop();
        }
    }

    void NodeImpl::spinImpl(const string& topic, const string& type_url, const function<void(const string&)>& cb) {
        auto io_queue = getRegistedTopicByPrefix(topic + "?" + type_url + "</");
        string msg;
        while (!io_queue.empty()) {
            while (io_queue.front().buf->try_dequeue(msg)) {
                cb(msg);
            }
            io_queue.pop();
        }
    }

    NodeImpl::~NodeImpl() {
        if (p2pServer) p2pServer->Shutdown();
        kill_regist_event();
    }
}

int main(int argc, char* argv[]) {
    auto node = make_shared<cos_core::NodeImpl>("/test$%/", argv[1]);
    node->subscribeImpl("/hello", "127.0.0.1", "google.protobuf.hello");
    node->advertiseImpl("/hello", "google.protobuf.hello");
    auto t = std::thread([&]() {
        uvw::loop::get_default()->run();
    });
    std::string largeString(512 * 512 * 1, 'A'); 
    while (1) {
        // LOG(INFO) << "send";
        node->publishImpl("/hello", "google.protobuf.hello", largeString);
        node->spinImpl("/hello", "google.protobuf.hello", [](const string&msg){LOG(INFO) << msg;});
        usleep(1000);
    }
    t.join();
    return 0;
}
