#ifndef REGISTER_HPP
#define REGISTER_HPP
/**/
#include "etcd/Client.hpp"
#include "etcd/SyncClient.hpp"
#include "etcd/Watcher.hpp"
#include "etcd/KeepAlive.hpp"

#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <queue>
#include <glog/logging.h>
#include "cos/p2p_event.hpp"

#define DEFAULT_ETCD_SRV_ADDR                   "http://127.0.0.1:2379"
#define WATCH_REGISTER_PREFIX                   "/registed/nodes"

namespace cos_core {

    using namespace std;

    class p2pClientgRPC;

    class Register {
    public:
        Register(const string& name, const int ttl = 1) ;
        ~Register();
        void                                        startRegist(const string& srv_addr);
        shared_ptr<p2pClientgRPC>                   handleRegistrationEvent(const p2pRegistrationEvent& event); // pop -> handle -> and do something else
        shared_ptr<p2pClientgRPC>                   getRemoteClientByName(const string& name);
        vector<shared_ptr<p2pClientgRPC>>           listAllExistClients(); // list -> get those clients to do something
        vector<string>                              listAllExistClientsName();
        void                                        kill_regist_event();
    protected:
        const string                                node_name;
    private:
        using RegistedClients = unordered_map<string, shared_ptr<p2pClientgRPC>>;
        shared_ptr<etcd::Client>                    etcd_client;
        shared_mutex                                registed_list_kv_mtx;
        RegistedClients                             registed_list_kv;
        
        const int                                   register_ttl;
        shared_ptr<etcd::Watcher>                   watcher;
        string                                      regist_key; 
        shared_ptr<p2pClientgRPC>                   put(const string& name, const string& srv_addr) ;
        shared_ptr<p2pClientgRPC>                   get(const string& name) ;
        void                                        remove(const string& name) ;
        void                                        regist_thread_callback(const string& srv_addr);
        void                                        wait_for_connection();
        void                                        etcd_regist_callback(etcd::Response const& resp);
        int64_t                                     get_preregisted_list();
        thread                                      regist_thread;
        const string                                split_name_with_prefix(const string& name);
    };
    
}

#endif