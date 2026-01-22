#include "network/connection.h"
#include <unistd.h>
#include <sstream>
#include <iostream>

namespace simpledb {
namespace network {

Connection::Connection(int socket_fd, std::shared_ptr<transaction::TransactionManager> txn_mgr,
                       std::shared_ptr<replication::CasPaxos> paxos)
    : socket_fd_(socket_fd), txn_mgr_(txn_mgr), paxos_(paxos), 
      current_txn_id_(0), in_transaction_(false) {
}

Connection::~Connection() {
    if (in_transaction_) {
        txn_mgr_->rollback_transaction(current_txn_id_);
    }
    close(socket_fd_);
}

void Connection::handle() {
    write_line("SimpleDB v1.0 - Ready");
    
    while (true) {
        std::string line = read_line();
        if (line.empty() || line == "QUIT") {
            break;
        }
        
        process_command(line);
    }
}

std::string Connection::read_line() {
    std::string line;
    char c;
    
    while (true) {
        ssize_t n = read(socket_fd_, &c, 1);
        if (n <= 0) {
            break;
        }
        
        if (c == '\n') {
            break;
        }
        
        if (c != '\r') {
            line += c;
        }
    }
    
    return line;
}

void Connection::write_line(const std::string& line) {
    std::string msg = line + "\r\n";
    write(socket_fd_, msg.c_str(), msg.length());
}

void Connection::process_command(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "GET") {
        std::string key;
        iss >> key;
        handle_get(key);
    } else if (cmd == "SET") {
        std::string key, value;
        iss >> key;
        std::getline(iss, value);
        // Trim leading space
        if (!value.empty() && value[0] == ' ') {
            value = value.substr(1);
        }
        handle_set(key, value);
    } else if (cmd == "DELETE") {
        std::string key;
        iss >> key;
        handle_delete(key);
    } else if (cmd == "CAS") {
        std::string key, old_value, new_value;
        iss >> key >> old_value;
        std::getline(iss, new_value);
        // Trim leading space
        if (!new_value.empty() && new_value[0] == ' ') {
            new_value = new_value.substr(1);
        }
        handle_cas(key, old_value, new_value);
    } else if (cmd == "BEGIN") {
        handle_begin();
    } else if (cmd == "COMMIT") {
        handle_commit();
    } else if (cmd == "ROLLBACK") {
        handle_rollback();
    } else {
        write_line("ERROR: Unknown command");
    }
}

void Connection::handle_get(const std::string& key) {
    // If CasPaxos is enabled, read from it
    if (paxos_) {
        auto value = paxos_->get(key);
        if (value.has_value()) {
            write_line("OK " + value.value());
        } else {
            write_line("NOT_FOUND");
        }
        return;
    }
    
    // Otherwise use transaction manager
    std::string value;
    
    if (in_transaction_) {
        if (txn_mgr_->read(current_txn_id_, key, value)) {
            write_line("OK " + value);
        } else {
            write_line("NOT_FOUND");
        }
    } else {
        // Auto-commit transaction for single operation
        uint64_t txn_id = txn_mgr_->begin_transaction();
        if (txn_mgr_->read(txn_id, key, value)) {
            write_line("OK " + value);
        } else {
            write_line("NOT_FOUND");
        }
        txn_mgr_->commit_transaction(txn_id);
    }
}

void Connection::handle_set(const std::string& key, const std::string& value) {
    if (in_transaction_) {
        if (txn_mgr_->write(current_txn_id_, key, value)) {
            write_line("OK");
        } else {
            write_line("ERROR: Write failed");
        }
    } else {
        // Auto-commit transaction for single operation
        uint64_t txn_id = txn_mgr_->begin_transaction();
        if (txn_mgr_->write(txn_id, key, value)) {
            txn_mgr_->commit_transaction(txn_id);
            write_line("OK");
        } else {
            txn_mgr_->rollback_transaction(txn_id);
            write_line("ERROR: Write failed");
        }
    }
}

void Connection::handle_delete(const std::string& key) {
    if (in_transaction_) {
        if (txn_mgr_->remove(current_txn_id_, key)) {
            write_line("OK");
        } else {
            write_line("ERROR: Delete failed");
        }
    } else {
        // Auto-commit transaction for single operation
        uint64_t txn_id = txn_mgr_->begin_transaction();
        if (txn_mgr_->remove(txn_id, key)) {
            txn_mgr_->commit_transaction(txn_id);
            write_line("OK");
        } else {
            txn_mgr_->rollback_transaction(txn_id);
            write_line("ERROR: Delete failed");
        }
    }
}

void Connection::handle_cas(const std::string& key, const std::string& old_value, 
                            const std::string& new_value) {
    if (!paxos_) {
        write_line("ERROR: CasPaxos not enabled");
        return;
    }
    
    if (in_transaction_) {
        write_line("ERROR: CAS not supported in transactions");
        return;
    }
    
    // Perform CAS operation
    std::optional<std::string> old_val;
    if (old_value != "null" && old_value != "NULL") {
        old_val = old_value;
    }
    
    bool success = paxos_->cas(key, old_val, new_value);
    if (success) {
        write_line("OK");
    } else {
        write_line("ERROR: CAS failed - condition not met or no quorum");
    }
}

void Connection::handle_begin() {
    if (in_transaction_) {
        write_line("ERROR: Already in transaction");
        return;
    }
    
    current_txn_id_ = txn_mgr_->begin_transaction();
    in_transaction_ = true;
    write_line("OK");
}

void Connection::handle_commit() {
    if (!in_transaction_) {
        write_line("ERROR: Not in transaction");
        return;
    }
    
    if (txn_mgr_->commit_transaction(current_txn_id_)) {
        write_line("OK");
    } else {
        write_line("ERROR: Commit failed");
    }
    
    in_transaction_ = false;
    current_txn_id_ = 0;
}

void Connection::handle_rollback() {
    if (!in_transaction_) {
        write_line("ERROR: Not in transaction");
        return;
    }
    
    if (txn_mgr_->rollback_transaction(current_txn_id_)) {
        write_line("OK");
    } else {
        write_line("ERROR: Rollback failed");
    }
    
    in_transaction_ = false;
    current_txn_id_ = 0;
}

} // namespace network
} // namespace simpledb
