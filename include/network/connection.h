#ifndef SIMPLE_DB_CONNECTION_H
#define SIMPLE_DB_CONNECTION_H

#include "transaction/transaction_manager.h"
#include <string>
#include <memory>

namespace simpledb {
namespace network {

// Connection handler for client connections
class Connection {
public:
    Connection(int socket_fd, std::shared_ptr<transaction::TransactionManager> txn_mgr);
    ~Connection();
    
    void handle();  // Main connection handler loop
    
private:
    int socket_fd_;
    std::shared_ptr<transaction::TransactionManager> txn_mgr_;
    uint64_t current_txn_id_;
    bool in_transaction_;
    
    std::string read_line();
    void write_line(const std::string& line);
    void process_command(const std::string& command);
    
    // Command handlers
    void handle_get(const std::string& key);
    void handle_set(const std::string& key, const std::string& value);
    void handle_delete(const std::string& key);
    void handle_begin();
    void handle_commit();
    void handle_rollback();
};

} // namespace network
} // namespace simpledb

#endif // SIMPLE_DB_CONNECTION_H
