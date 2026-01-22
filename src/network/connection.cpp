#include "network/connection.h"
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace simpledb {
namespace network {

Connection::Connection(int socket_fd, std::shared_ptr<transaction::TransactionManager> txn_mgr)
    : socket_fd_(socket_fd), txn_mgr_(txn_mgr), current_txn_id_(0), in_transaction_(false) {
}

Connection::~Connection() {
    if (in_transaction_) {
        txn_mgr_->rollback_transaction(current_txn_id_);
    }
    close(socket_fd_);
}

void Connection::handle() {
    write_line("SimpleDB v2.0 - Vector Database with HNSW - Ready");
    
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

storage::Vector Connection::parse_vector(const std::string& vec_str) {
    storage::Vector vec;
    std::string cleaned = vec_str;
    
    // Remove brackets if present
    if (!cleaned.empty() && cleaned[0] == '[') {
        cleaned = cleaned.substr(1);
    }
    if (!cleaned.empty() && cleaned.back() == ']') {
        cleaned = cleaned.substr(0, cleaned.length() - 1);
    }
    
    // Parse comma-separated floats
    std::istringstream iss(cleaned);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try {
            vec.push_back(std::stof(token));
        } catch (...) {
            // Skip invalid values
        }
    }
    
    return vec;
}

std::string Connection::vector_to_string(const storage::Vector& vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << std::fixed << std::setprecision(6) << vec[i];
    }
    oss << "]";
    return oss.str();
}

void Connection::process_command(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "GET") {
        std::string key;
        iss >> key;
        handle_get(key);
    } else if (cmd == "INSERT") {
        std::string key;
        iss >> key;
        std::string vector_str;
        std::getline(iss, vector_str);
        // Trim leading space
        if (!vector_str.empty() && vector_str[0] == ' ') {
            vector_str = vector_str.substr(1);
        }
        handle_insert(key, vector_str);
    } else if (cmd == "SEARCH") {
        std::string vector_str;
        std::getline(iss, vector_str);
        // Trim leading space
        if (!vector_str.empty() && vector_str[0] == ' ') {
            vector_str = vector_str.substr(1);
        }
        
        // Extract k value if present
        size_t k = 10;  // default
        size_t k_pos = vector_str.find("TOP");
        if (k_pos != std::string::npos) {
            std::string vec_part = vector_str.substr(0, k_pos);
            std::string k_part = vector_str.substr(k_pos + 3);
            std::istringstream k_iss(k_part);
            k_iss >> k;
            vector_str = vec_part;
        }
        
        handle_search(vector_str, k);
    } else if (cmd == "DELETE") {
        std::string key;
        iss >> key;
        handle_delete(key);
    } else if (cmd == "BEGIN") {
        handle_begin();
    } else if (cmd == "COMMIT") {
        handle_commit();
    } else if (cmd == "ROLLBACK") {
        handle_rollback();
    } else {
        write_line("ERROR: Unknown command. Available: INSERT, GET, SEARCH, DELETE, BEGIN, COMMIT, ROLLBACK, QUIT");
    }
}

void Connection::handle_get(const std::string& key) {
    storage::Vector vector;
    
    if (in_transaction_) {
        if (txn_mgr_->read(current_txn_id_, key, vector)) {
            write_line("OK " + vector_to_string(vector));
        } else {
            write_line("NOT_FOUND");
        }
    } else {
        // Auto-commit transaction for single operation
        uint64_t txn_id = txn_mgr_->begin_transaction();
        if (txn_mgr_->read(txn_id, key, vector)) {
            write_line("OK " + vector_to_string(vector));
        } else {
            write_line("NOT_FOUND");
        }
        txn_mgr_->commit_transaction(txn_id);
    }
}

void Connection::handle_insert(const std::string& key, const std::string& vector_str) {
    storage::Vector vector = parse_vector(vector_str);
    
    if (vector.empty()) {
        write_line("ERROR: Invalid vector format. Use: INSERT key [v1,v2,v3,...]");
        return;
    }
    
    if (in_transaction_) {
        if (txn_mgr_->write(current_txn_id_, key, vector)) {
            write_line("OK");
        } else {
            write_line("ERROR: Insert failed");
        }
    } else {
        // Auto-commit transaction for single operation
        uint64_t txn_id = txn_mgr_->begin_transaction();
        if (txn_mgr_->write(txn_id, key, vector)) {
            txn_mgr_->commit_transaction(txn_id);
            write_line("OK");
        } else {
            txn_mgr_->rollback_transaction(txn_id);
            write_line("ERROR: Insert failed");
        }
    }
}

void Connection::handle_search(const std::string& vector_str, size_t k) {
    storage::Vector query = parse_vector(vector_str);
    
    if (query.empty()) {
        write_line("ERROR: Invalid vector format. Use: SEARCH [v1,v2,v3,...] TOP k");
        return;
    }
    
    auto results = txn_mgr_->search(query, k);
    
    if (results.empty()) {
        write_line("OK 0 results");
    } else {
        std::ostringstream oss;
        oss << "OK " << results.size() << " results";
        for (const auto& result : results) {
            oss << "\r\n" << result.key << " distance=" << std::fixed << std::setprecision(6) << result.distance;
        }
        write_line(oss.str());
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
