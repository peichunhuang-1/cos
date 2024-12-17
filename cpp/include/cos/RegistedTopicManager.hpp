#ifndef REGISTED_TOPIC_MANAGER_HPP
#define REGISTED_TOPIC_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include <uvw.hpp>
#include "cos/utils/trie.hpp"
#include "concurrentqueue.h"
#include <shared_mutex>

#define APPEND_SIZE                 128
#define MAX_AUTHORIZE_MSG_LEN       2048


namespace cos_core {
    using namespace std;

    struct uv_io_t {
    public:
        using buffer_t = moodycamel::ConcurrentQueue<string>;
        uv_io_t(): buf(make_shared<buffer_t>()) {}
        uv_io_t(const string& ip_, const int& port_): ip(ip_), port(port_), buf(make_shared<buffer_t>()) {}
        string                          ip = "";
        int                             port = 0;
        shared_ptr<buffer_t>            buf;
        shared_ptr<uvw::async_handle>   trigger;
    };

    class RegistedTopicManager {
    public:
        RegistedTopicManager() ;
        /* insert local publisher in trie, and reserve space for lookup, send notify to peers */
        void                        insertLocalPublishedTopic(const string& topic, const string& type_url);
        /* insert local subscriber in trie, create tcp server, update addr for trie lookup, and send notify to peers */
        void                        insertLocalSubscribedTopic(const string& topic, const string& type_url, const string& ip, int& port);
        /* receive from remote publisher notify, if local subscriber found and not yet subscribed to the remote node, 
        get srv info, and call resend subscribe notify */
        bool                        insertRemotePublishedTopic(const string& remote_node, const string& topic, const string& type_url, 
                                    string& local_srv_ip, int& local_srv_port);
        /* receive from remote subscriber notify, if local publisher found, call modify local publisher */
        bool                        insertRemoteSubscribedTopic(const string& remote_node, const string& topic, const string& type_url, 
                                    const string& remote_srv_ip, const int& remote_srv_port);
        /* create tcp connection and get info of client, send authorized call to remote subscriber */
        void                        modifyLocalPublishedTopic(const string& local_node, const string& remote_node, const string& topic, const string& type_url, 
                                    string& local_clt_ip, int& local_clt_port);
        /* receive authorized call from remote publisher, modify trie for remote pub and local sub */
        void                        modifyLocalSubscribedTopic(const string& remote_node, const string& topic, const string& type_url, 
                                    const string& remote_clt_ip, const int& remote_clt_port);

        void                        removeLocalPublishedTopic(const string& topic, const string& type_url);
        void                        removeLocalSubscribedTopic(const string& topic, const string& type_url);
        void                        removeRemotePublishedTopic(const string& remote_node, const string& topic, const string& type_url);
        void                        removeRemoteSubscribedTopic(const string& remote_node, const string& topic, const string& type_url);
        void                        removeRemoteNode(const string& remote_node);
        void                        listAllLocalTopic(queue<string>& published, queue<string>& subscribed);
        bool                        getRegistedTopicByKey(const string& key, uv_io_t& val);
        queue<uv_io_t>              getRegistedTopicByPrefix(const string& prefix);
    private:
        /*
        format:
        1. publisher: topic + ? + type_url + >* -> null
        2. subscriber: topic + ? + type_url + <* -> server io
        3. publisher: topic + ? + type_url + > + remote -> writer io
        4. subscriber: topic + ? + type_url + < + remote -> reader io
        5. remote publisher: remote + > + topic + ? + type_url -> addr
        6. remote subscriber: remote + < + topic + ? + type_url -> addr
        */
        cos_utils::triemap          lookup_table;
        shared_mutex                mtx;
        vector<uv_io_t>             uv_io;
        void                        create_tcp_server(const string& topic, const string& type_url, const string& ip, int& port);
        void                        create_tcp_client(const string& local_node, const string& remote_node, const string& topic, const string& type_url, const string& srv_ip, const int& srv_port, string& clt_ip, int& clt_port);
    };
}

#endif