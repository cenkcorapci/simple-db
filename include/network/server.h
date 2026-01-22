#ifndef SIMPLE_DB_SERVER_H
#define SIMPLE_DB_SERVER_H

#include "network/connection.h"
#include "transaction/transaction_manager.h"
#include "replication/caspaxos.h"
#include <memory>
#include <atomic>
#include <thread>
#include <vector>

namespace simpledb {
namespace network {

// TCP Server for handling concurrent connections
class Server {
public:
    Server(int port, std::shared_ptr<transaction::TransactionManager> txn_mgr,
           std::shared_ptr<replication::CasPaxos> paxos = nullptr);
    ~Server();
    
    void start();
    void stop();
    bool is_running() const { return running_; }
    
private:
    int port_;
    int server_fd_;
    std::shared_ptr<transaction::TransactionManager> txn_mgr_;
    std::shared_ptr<replication::CasPaxos> paxos_;
    std::atomic<bool> running_;
    std::vector<std::thread> worker_threads_;
    
    void accept_connections();
    void handle_connection(int client_fd);
    int create_server_socket();
};

} // namespace network
} // namespace simpledb

#endif // SIMPLE_DB_SERVER_H
