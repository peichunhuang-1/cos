#include <chrono>
#include <iostream>
#include <thread>

#include "etcd/Client.hpp"

int main() {

    static const std::string etcd_url =
    etcdv3::detail::resolve_etcd_endpoints("http://127.0.0.1:2379");
    etcd::Client etcd(etcd_url);
    etcd.rmdir("/test", true).wait();
    etcd::Response resp = etcd.add("/test/key1", "42").get();
    etcd::Value const& val = resp.value();
    std::cout << val.key() << "\n";
    return 0;
}