#include "cos/Register.hpp"
#include "cos/p2pClientgRPC.hpp"
namespace cos_core {
    Register::Register(const string& name, const int ttl): register_ttl(ttl), node_name(name) {}

    void Register::startRegist(const string& srv_addr) {
        DLOG(INFO) << "Waiting for etcd connection ...";
        wait_for_connection();
        DLOG(INFO) << "Connection established";
        regist_thread = thread(&Register::regist_thread_callback, this, srv_addr);
    }

    const string Register::split_name_with_prefix(const string& name) {
        return name.substr(strlen(WATCH_REGISTER_PREFIX));
    }

    void Register::wait_for_connection() { // wait for etcd connection
        const string etcd_srv_addr = etcdv3::detail::resolve_etcd_endpoints(
            string(getenv("ETCD_SRV_ADDR") ? getenv("ETCD_SRV_ADDR") : DEFAULT_ETCD_SRV_ADDR));
        while (true) {
            DLOG(INFO) << "Try connecting to etcd server on " << etcd_srv_addr;
        #ifndef _ETCD_NO_EXCEPTIONS
            try {
            etcd_client.reset(new etcd::Client(etcd_srv_addr));
            if (etcd_client->head().get().is_ok()) break;
            } catch (...) {
                DLOG(ERROR) << "Unknown error occur when trying to connect to etcd server";
            }
        #else
            etcd_client.reset(new etcd::Client(etcd_srv_addr));
            if (etcd_client->head().get().is_ok()) {
            break;
            }
        #endif
            sleep(1);
        }
    }

    shared_ptr<p2pClientgRPC> Register::put(const string& name, const string& srv_addr) {
        unique_lock<shared_mutex> lock(registed_list_kv_mtx);
        registed_list_kv[name] = make_shared<p2pClientgRPC>(name, node_name, srv_addr);
        return registed_list_kv[name];
    }

    shared_ptr<p2pClientgRPC> Register::get(const string& name) {
        unique_lock<shared_mutex> lock(registed_list_kv_mtx);
        if (registed_list_kv.count(name)) return registed_list_kv[name];
        else return nullptr;
    }

    void Register::remove(const string& name) {
        unique_lock<shared_mutex> lock(registed_list_kv_mtx);
        if (registed_list_kv.count(name)) {
            registed_list_kv.erase(name);
        }
    }

    int64_t Register::get_preregisted_list() {
        etcd::Response resp = etcd_client->ls(WATCH_REGISTER_PREFIX).get();
        if (resp.error_code()) {
            DLOG(ERROR) << "Error: " << resp.error_message() << ", occur when trying to list storage in " << WATCH_REGISTER_PREFIX;
            exit(-1);
        }
        int64_t revision = 0;
        for (int i = 0; i < resp.keys().size(); i++) {
            p2pEventPools::pushEvent(node_name, p2pEvent::p2pRegistration(split_name_with_prefix(resp.key(i)), resp.value(i).as_string(), p2pRegistrationEvent::opt::CREATE));
            revision = max(resp.value(i).modified_index(), revision);
        }
        return revision;
    }

    void Register::regist_thread_callback(const string& srv_addr) {
        etcd::KeepAlive keepalive(*etcd_client, register_ttl);
        auto lease_id = keepalive.Lease();
        if (lease_id == 0) {
            DLOG(ERROR) << "Error create auto regranted lease";
            exit(-1);
        }
        regist_key = 
        !node_name.empty() && node_name[0] == '/' 
            ? string(WATCH_REGISTER_PREFIX) + node_name 
            : string(WATCH_REGISTER_PREFIX) + "/" + node_name;
        etcd::Response resp = etcd_client->add(regist_key, srv_addr, lease_id).get();
        
        if (!resp.error_code()) {
            int64_t revision = get_preregisted_list();
            watcher.reset( new etcd::Watcher(*etcd_client, WATCH_REGISTER_PREFIX, revision + 1, ::bind(&Register::etcd_regist_callback, this, placeholders::_1), true));
            watcher->Wait(); // should cancel in other place
            keepalive.Cancel();
        } else {
            if (etcd::ERROR_KEY_ALREADY_EXISTS == resp.error_code()) {
                DLOG(WARNING) << "Warning: " << regist_key << " | etcd::ERROR_KEY_ALREADY_EXISTS, retry and wait for deregisting";
                sleep(1);
                regist_thread_callback(srv_addr); // retry with delay
            } else {
                DLOG(ERROR) << "Error: " << resp.error_message() << ", occur when trying to add key: " << regist_key;
                exit(-1);
            }
        }
    }

    void Register::etcd_regist_callback(etcd::Response const& resp) {
        if (resp.error_code()) {
            DLOG(ERROR) << "Error: " << resp.error_message();
        } else {
            for (auto const& ev : resp.events()) {
                const string name = split_name_with_prefix(ev.kv().key());
                const string srv_addr = ev.kv().as_string();
                if (ev.event_type() == etcd::Event::EventType::PUT) {
                    p2pEventPools::pushEvent(node_name, p2pEvent::p2pRegistration(name, srv_addr, p2pRegistrationEvent::opt::CREATE));
                } else if (ev.event_type() == etcd::Event::EventType::DELETE_) {
                    p2pEventPools::pushEvent(node_name, p2pEvent::p2pRegistration(name, srv_addr, p2pRegistrationEvent::opt::DELETE));
                } else {
                    DLOG(WARNING) << "Warning: Invalid type event, passing";
                }
            }
        }
    }

    shared_ptr<p2pClientgRPC> Register::handleRegistrationEvent(const p2pRegistrationEvent& event) {
        if (event.operation == p2pRegistrationEvent::opt::CREATE) {
            return this->put(event.name, event.srv_addr);
        } else {
            this->remove(event.name);
            return nullptr;
        }
    }

    shared_ptr<p2pClientgRPC> Register::getRemoteClientByName(const string& name) {
        return this->get(name);
    }

    vector<shared_ptr<p2pClientgRPC>> Register::listAllExistClients() {
        vector<shared_ptr<p2pClientgRPC>> list;
        unique_lock<shared_mutex> lock(registed_list_kv_mtx);
        for (auto& kv: registed_list_kv) {
            list.push_back(kv.second);
        }
        return list;
    }

    vector<string> Register::listAllExistClientsName() {
        vector<string> list;
        unique_lock<shared_mutex> lock(registed_list_kv_mtx);
        for (auto& kv: registed_list_kv) {
            list.push_back(kv.first);
        }
        return list;
    }

    void Register::kill_regist_event() {
        etcd::Response resp = etcd_client->rm(regist_key).get();
        if (resp.is_ok()) DLOG(INFO) << "Successfully deregisted " <<  regist_key;
        else DLOG(ERROR) << "Failed to deregist " <<  regist_key;
        watcher->Cancel();
    }
    Register::~Register() {
        if (regist_thread.joinable()) {
            regist_thread.join();
        }
    }
}