#include "cos/p2p_event.hpp"

namespace cos_core {

    p2pEvent::p2pEvent(const p2pEventType& ev_type) : type(ev_type) {}

    const p2pEventType p2pEvent::getType() {
        return type;
    }

    p2pEvent p2pEvent::empty() {
        p2pEvent e(p2pEventType::EMPTY);
        return move(e);
    }

    p2pEvent p2pEvent::p2pRegistration(const string& name, const string& srv_addr, const p2pRegistrationEvent::opt& opt) {
        p2pEvent e(p2pEventType::REGISTRATION);
        e.event = p2pRegistrationEvent {name, srv_addr, opt};
        return move(e);
    }

    p2pEvent p2pEvent::p2pAddRemoteSubscriber(const string& remote_name, const string& topic, const string& remote_ip, const int& remote_port, const string& type_url) {
        p2pEvent e(p2pEventType::ADD_REMOTE_SUBSCRIBER);
        e.event = p2pAddRemoteSubscriberEvent {remote_name, topic, remote_ip, remote_port, type_url};
        return move(e);
    }

    p2pEvent p2pEvent::p2pAddRemotePublisher(const string& remote_name, const string& topic, const string& type_url) {
        p2pEvent e(p2pEventType::ADD_REMOTE_PUBLISHER);
        e.event = p2pAddRemotePublisherEvent {remote_name, topic, type_url};
        return move(e);
    }

    p2pEvent p2pEvent::p2pAddLocalSubscriber(const string& topic, const string& local_ip, const string& type_url) {
        p2pEvent e(p2pEventType::ADD_LOCAL_SUBSCRIBER);
        e.event = p2pAddLocalSubscriberEvent {topic, local_ip, type_url};
        return move(e);
    }

    p2pEvent p2pEvent::p2pAddLocalPublisher(const string& topic, const string& type_url) {
        p2pEvent e(p2pEventType::ADD_LOCAL_PUBLISHER);
        e.event = p2pAddLocalPublisherEvent {topic, type_url};
        return move(e);
    }

    p2pEvent p2pEvent::p2pAuthorizeIO(const string& remote_name, const string& topic, const string& ip, const int& port, const string& type_url) {
        p2pEvent e(p2pEventType::AUTHORIZE_IO);
        e.event = p2pAuthorizeIOEvent {remote_name, topic, ip, port, type_url};
        return move(e);
    }

    unordered_map<string, queue<p2pEvent>> p2pEventPools::node_pools_event_queue;
    shared_mutex p2pEventPools::node_pools_event_queue_mtx;

    void p2pEventPools::pushEvent(const string& node, const p2pEvent& event) {
        unique_lock<shared_mutex> lock(p2pEventPools::node_pools_event_queue_mtx);
        p2pEventPools::node_pools_event_queue[node].push(event);
    }

    bool p2pEventPools::popEvent(const string& node, p2pEvent& event) {
        if (isQueueEmpty(node)) return false;
        unique_lock<shared_mutex> lock(p2pEventPools::node_pools_event_queue_mtx);
        event = p2pEventPools::node_pools_event_queue[node].front();
        p2pEventPools::node_pools_event_queue[node].pop();
        return true;
    }

    bool p2pEventPools::isQueueEmpty(const string& node) {
        shared_lock<shared_mutex> lock(p2pEventPools::node_pools_event_queue_mtx);
        if (!p2pEventPools::node_pools_event_queue.count(node)) return true;
        if (p2pEventPools::node_pools_event_queue[node].empty()) return true;
        else return false;
    }
    
}