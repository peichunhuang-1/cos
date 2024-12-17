#ifndef EVENT_POOL_HPP
#define EVENT_POOL_HPP
#include <queue>
#include <unordered_map>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <iostream>
#include <variant>
#include <optional>

namespace cos_core {
    using namespace std;

    struct p2pRegistrationEvent {
    enum opt {DELETE = 0, CREATE=1};
    public:
        string                                      name;
        string                                      srv_addr;
        opt                                         operation;
    };

    struct p2pAddRemoteSubscriberEvent {
    public:
        string                                      remote_name;
        string                                      topic;
        string                                      remote_ip;
        int                                         remote_port;
        string                                      type_url;
    };

    struct p2pAddRemotePublisherEvent {
    public:
        string                                      remote_name;
        string                                      topic;
        string                                      type_url;
    };

    struct p2pAddLocalSubscriberEvent {
    public:
        string                                      topic;
        string                                      local_ip;
        string                                      type_url;
    };

    struct p2pAddLocalPublisherEvent {
    public:
        string                                      topic;
        string                                      type_url;
    };

    struct p2pAuthorizeIOEvent {
    public:
        string                                      remote_name;
        string                                      topic;
        string                                      ip;
        int                                         port;
        string                                      type_url;
    };

    enum class p2pEventType {
        EMPTY                                   =   0,
        REGISTRATION                            =   1,
        ADD_REMOTE_SUBSCRIBER                   =   2,
        ADD_REMOTE_PUBLISHER                    =   3,
        ADD_LOCAL_SUBSCRIBER                    =   4,
        ADD_LOCAL_PUBLISHER                     =   5,
        AUTHORIZE_IO                            =   6
    };


    class p2pEvent {
    public:
        using p2pEventUnion = variant<monostate, p2pRegistrationEvent, p2pAddRemoteSubscriberEvent, p2pAddRemotePublisherEvent, p2pAddLocalSubscriberEvent, p2pAddLocalPublisherEvent, p2pAuthorizeIOEvent>;
        p2pEvent(const p2pEvent& other)            =   default;
        ~p2pEvent()                                =   default;
        static p2pEvent                                empty();
        static p2pEvent                                p2pRegistration(const string& name, const string& srv_addr, const p2pRegistrationEvent::opt& opt);
        static p2pEvent                                p2pAddRemoteSubscriber(const string& remote_name, const string& topic, const string& remote_ip, const int& remote_port, const string& type_url);
        static p2pEvent                                p2pAddRemotePublisher(const string& remote_name, const string& topic, const string& type_url);
        static p2pEvent                                p2pAddLocalSubscriber(const string& topic, const string& local_ip, const string& type_url);
        static p2pEvent                                p2pAddLocalPublisher(const string& topic, const string& type_url);
        static p2pEvent                                p2pAuthorizeIO(const string& remote_name, const string& topic, const string& ip, const int& port, const string& type_url);
        p2pEventUnion                                  event;
        const p2pEventType                             getType();
    private:
        explicit p2pEvent(const p2pEventType& ev_type);
        p2pEventType                                   type;
    };

    class p2pEventPools {
    public:
        static void pushEvent(const string& node, const p2pEvent& event);
        static bool popEvent(const string& node, p2pEvent& event);
        static bool isQueueEmpty(const string& node);
 
        static unordered_map<string, queue<p2pEvent>>      node_pools_event_queue;
        static shared_mutex                             node_pools_event_queue_mtx;
    };
    
}

#endif