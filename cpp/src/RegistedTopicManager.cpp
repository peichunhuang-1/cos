#include "cos/RegistedTopicManager.hpp"
#include <iostream>
namespace cos_core {
    RegistedTopicManager::RegistedTopicManager() {}

    void RegistedTopicManager::create_tcp_server(const string& topic, const string& type_url, const string& ip, int& port) {
        auto tcp = uvw::loop::get_default()->resource<uvw::tcp_handle>();
        tcp->on<uvw::error_event>([](const auto &, auto &) { });
        tcp->bind(ip, 0);
        auto addr = tcp->sock();
        port = addr.port;
        tcp->on<uvw::listen_event>([topic, type_url, this](const uvw::listen_event &, uvw::tcp_handle &server) {
            auto client = server.parent().resource<uvw::tcp_handle>();
            server.accept(*client);
            auto authorized = make_shared<bool>(false);
            auto tmp_buf = make_shared<string>();
            auto io_ptr = make_shared<uv_io_t>();
            auto timeout_timer = uvw::loop::get_default()->resource<uvw::timer_handle>();
            timeout_timer->start(uvw::timer_handle::time{5000}, uvw::timer_handle::time{0});
            timeout_timer->on<uvw::timer_event>([&client, authorized, timeout_timer](const uvw::timer_event &, uvw::timer_handle &) {
                if (!(*authorized)) {
                    client->close();
                    timeout_timer->close();
                }
            });
            client->on<uvw::data_event>([topic, type_url, authorized, tmp_buf, io_ptr, timeout_timer, this](const uvw::data_event &event, uvw::tcp_handle &client) {
                string msg{event.data.get(), event.length};
                if (*authorized) {
                    tmp_buf->append(msg.data(), msg.length());
                    uint32_t msgLength;
                    while (1) {
                        if (tmp_buf->length() > sizeof(uint32_t)) memcpy(&msgLength, tmp_buf->data(), sizeof(uint32_t));
                        else break;
                        if (tmp_buf->length() < sizeof(uint32_t) + msgLength) break;
                        io_ptr->buf->enqueue(tmp_buf->substr(sizeof(uint32_t), msgLength));
                        tmp_buf->erase(0, sizeof(uint32_t) + msgLength);
                    }
                    return;
                }
                tmp_buf->append(msg.data(), msg.length());
                uint32_t keyLength;
                if (tmp_buf->length() > sizeof(uint32_t)) memcpy(&keyLength, tmp_buf->data(), sizeof(uint32_t));
                else return;
                if (keyLength > MAX_AUTHORIZE_MSG_LEN) {
                    client.close();
                    return;
                }
                if (tmp_buf->length() < sizeof(uint32_t) + keyLength) return;
                string key = tmp_buf->substr(sizeof(uint32_t), keyLength);
                uv_io_t io;
                if (this->getRegistedTopicByKey(key, io) && client.peer().ip == io.ip && client.peer().port == io.port) {
                    *authorized = true;
                    *io_ptr = io;
                    tmp_buf->erase(0, keyLength + sizeof(uint32_t));
                    timeout_timer->close();
                }
                return;
            });
            client->on<uvw::end_event>([](const uvw::end_event &, uvw::tcp_handle &client) {
                client.close();
            });
            client->read();
        });
        tcp->listen();
    }

    void RegistedTopicManager::create_tcp_client(const string& local_node, const string& remote_node, const string& topic, const string& type_url, const string& srv_ip, const int& srv_port, string& clt_ip, int& clt_port) {
        auto client = uvw::loop::get_default()->resource<uvw::tcp_handle>();
        int ind = lookup_table.find(topic + "?" + type_url + ">" + remote_node);
        auto trigger = uvw::loop::get_default()->resource<uvw::async_handle>();
        uv_io[ind].trigger = trigger;
        auto buf = uv_io[ind].buf;
        client->on<uvw::error_event>([](const auto &, auto &) { });
        auto is_connected = std::make_shared<std::atomic<bool>>(false);
        client->on<uvw::connect_event>([local_node, topic, type_url, trigger, is_connected](const uvw::connect_event &, uvw::tcp_handle &handle) {
            string message = topic + "?" + type_url + "<" + local_node;
            uint32_t messageLength = static_cast<uint32_t>(message.size());
            auto dataWrite = make_unique<char[]>(sizeof(uint32_t) + messageLength);
            memcpy(dataWrite.get(), &messageLength, sizeof(uint32_t));
            memcpy(dataWrite.get() + sizeof(uint32_t), message.c_str(), messageLength);
            handle.write(move(dataWrite), messageLength + sizeof(uint32_t));
            *is_connected = true;
        });
        trigger->on<uvw::async_event>([client, buf, is_connected] (uvw::async_event &, uvw::async_handle &) {
            if (!*is_connected) return;
            string msg;
            while (buf->try_dequeue(msg)) {
                uint32_t messageLength = static_cast<uint32_t>(msg.size());
                auto dataWrite = std::make_unique<char[]>(sizeof(uint32_t) + messageLength);
                std::memcpy(dataWrite.get(), &messageLength, sizeof(uint32_t));
                std::memcpy(dataWrite.get() + sizeof(uint32_t), msg.c_str(), messageLength);
                client->write(std::move(dataWrite), messageLength + sizeof(uint32_t));
            }
        });
        client->connect(srv_ip, srv_port);
        auto localAddr = client->sock();
        clt_ip = localAddr.ip;
        clt_port = localAddr.port;
    }
    
    void RegistedTopicManager::insertLocalPublishedTopic(const string& topic, const string& type_url) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.insert(topic + "?" + type_url + ">*");
        if (ind > 0) {
            if (ind >= uv_io.size()) {
                uv_io.resize(uv_io.size() + APPEND_SIZE); 
            }
        }
    }

    void RegistedTopicManager::insertLocalSubscribedTopic(const string& topic, const string& type_url, const string& ip, int& port) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.insert(topic + "?" + type_url + "<*");
        if (ind > 0) {
            if (ind >= uv_io.size()) {
                uv_io.resize(uv_io.size() + APPEND_SIZE); 
            }
            create_tcp_server(topic, type_url, ip, port);
            uv_io[ind] = uv_io_t(ip, port);
        }
    }

    bool RegistedTopicManager::insertRemotePublishedTopic(const string& remote_node, const string& topic, const string& type_url, string& local_srv_ip, int& local_srv_port) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.insert(remote_node + ">" + topic + "?" + type_url);
        if (ind > 0) {
            if (ind >= uv_io.size()) {
                uv_io.resize(uv_io.size() + APPEND_SIZE); 
            }
            ind = lookup_table.find(topic + "?" + type_url + "<*");
            if (ind > 0 && lookup_table.find(topic + "?" + type_url + "<" + remote_node) <= 0) {
                local_srv_ip = uv_io[ind].ip;
                local_srv_port = uv_io[ind].port;
                return true;
            } else {
                return false;
            }
        } else return false;
    }

    bool RegistedTopicManager::insertRemoteSubscribedTopic(const string& remote_node, const string& topic, const string& type_url, const string& remote_srv_ip, const int& remote_srv_port) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.insert(remote_node + "<" + topic + "?" + type_url);
        if (ind > 0) {
            if (ind >= uv_io.size()) {
                uv_io.resize(uv_io.size() + APPEND_SIZE); 
            }
            uv_io[ind] = uv_io_t(remote_srv_ip, remote_srv_port);
            ind = lookup_table.find(topic + "?" + type_url + ">*");
            if (ind > 0 && lookup_table.find(topic + "?" + type_url + ">" + remote_node) <= 0) return true;
            else return false;
        }
        else return false;
    }

    void RegistedTopicManager::modifyLocalPublishedTopic(const string& local_node, const string& remote_node, const string& topic, const string& type_url, string& local_clt_ip, int& local_clt_port) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.insert(topic + "?" + type_url + ">" + remote_node);
        if (ind > 0) {
            if (ind >= uv_io.size()) {
                uv_io.resize(uv_io.size() + APPEND_SIZE); 
            }
            int remote_ind = lookup_table.find(remote_node + "<" + topic + "?" + type_url);
            if (remote_ind < 0) return;
            create_tcp_client(local_node, remote_node, topic, type_url, uv_io[remote_ind].ip, uv_io[remote_ind].port, local_clt_ip, local_clt_port);
            uv_io[ind].ip = local_clt_ip;
            uv_io[ind].port = local_clt_port;
        }
    }

    void RegistedTopicManager::modifyLocalSubscribedTopic(const string& remote_node, const string& topic, const string& type_url, const string& remote_clt_ip, const int& remote_clt_port) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.insert(topic + "?" + type_url + "<" + remote_node);
        if (ind > 0) {
            if (ind >= uv_io.size()) {
                uv_io.resize(uv_io.size() + APPEND_SIZE); 
            }
            uv_io[ind] = uv_io_t(remote_clt_ip, remote_clt_port);
        }
    }

    void RegistedTopicManager::removeLocalPublishedTopic(const string& topic, const string& type_url) {
        unique_lock<shared_mutex> lock(mtx);
        queue<string> suffixs;
        auto erased_ind = lookup_table.prefixEraseAll(topic + "?" + type_url + ">", suffixs);
        for (auto ind: erased_ind) uv_io[ind] = uv_io_t();
        while (!suffixs.empty()) {
            removeRemoteSubscribedTopic(topic, type_url, suffixs.front());
            suffixs.pop();
        }
    }

    void RegistedTopicManager::removeLocalSubscribedTopic(const string& topic, const string& type_url) {
        unique_lock<shared_mutex> lock(mtx);
        queue<string> suffixs;
        auto erased_ind = lookup_table.prefixEraseAll(topic + "?" + type_url + "<", suffixs);
        for (auto ind: erased_ind) uv_io[ind] = uv_io_t();
        while (!suffixs.empty()) {
            removeRemotePublishedTopic(topic, type_url, suffixs.front());
            suffixs.pop();
        }
    }

    void RegistedTopicManager::removeRemotePublishedTopic(const string& remote_node, const string& topic, const string& type_url) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.erase(remote_node + ">" + topic + "?" + type_url);
        if (ind > 0) uv_io[ind] = uv_io_t();
        ind = lookup_table.erase(topic + "?" + type_url + "<" + remote_node);
        if (ind > 0) uv_io[ind] = uv_io_t();
    }

    void RegistedTopicManager::removeRemoteSubscribedTopic(const string& remote_node, const string& topic, const string& type_url) {
        unique_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.erase(remote_node + "<" + topic + "?" + type_url);
        if (ind > 0) uv_io[ind] = uv_io_t();
        ind = lookup_table.erase(topic + "?" + type_url + ">" + remote_node);
        if (ind > 0) uv_io[ind] = uv_io_t();
    }

    void RegistedTopicManager::removeRemoteNode(const string& remote_node) {
        unique_lock<shared_mutex> lock(mtx);
        queue<string> suffixs;
        auto erased_ind = lookup_table.prefixEraseAll(remote_node + "<", suffixs);
        for (auto ind: erased_ind) uv_io[ind] = uv_io_t();
        while (!suffixs.empty()) {
            int ind = lookup_table.erase(suffixs.front() + ">" + remote_node);
            if (ind > 0) uv_io[ind] = uv_io_t();
            suffixs.pop();
        }
        erased_ind = lookup_table.prefixEraseAll(remote_node + ">", suffixs);
        for (auto ind: erased_ind) uv_io[ind] = uv_io_t();
        while (!suffixs.empty()) {
            int ind = lookup_table.erase(suffixs.front() + "<" + remote_node);
            if (ind > 0) uv_io[ind] = uv_io_t();
            suffixs.pop();
        }
    }

    void RegistedTopicManager::listAllLocalTopic(queue<string>& published, queue<string>& subscribed) {
        shared_lock<shared_mutex> lock(mtx);
        queue<string> all_topics;
        lookup_table.listKey(all_topics, "/");
        while (!all_topics.empty()) {
            const string topic = all_topics.front();
            size_t found = topic.find(">*");
            if (found == string::npos) {
                found = topic.find("<*");
                if (found != string::npos) { // subscriber found
                    subscribed.push(topic.substr(0, found));
                }
            } else { // publisher found
                published.push(topic.substr(0, found));
            }
            all_topics.pop();
        }
    }

    bool RegistedTopicManager::getRegistedTopicByKey(const string& key, uv_io_t& val) {
        shared_lock<shared_mutex> lock(mtx);
        int ind = lookup_table.find(key);
        if (ind > 0) {
            val = uv_io[ind];
            return true;
        }
        return false;
    }

    queue<uv_io_t> RegistedTopicManager::getRegistedTopicByPrefix(const string& prefix) {
        shared_lock<shared_mutex> lock(mtx);
        queue<int> founded;
        queue<uv_io_t> registed_ios; 
        lookup_table.listValue(founded, prefix);
        while (!founded.empty()) {
            registed_ios.push(uv_io[founded.front()]);
            founded.pop();
        }
        return registed_ios;
    }

}