#include "network/server.h"
#include "transaction/transaction_manager.h"
#include "storage/kv_store.h"
#include "replication/replicator.h"
#include <iostream>
#include <memory>
#include <signal.h>
#include <cstring>

using namespace simpledb;

static std::unique_ptr<network::Server> g_server;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
        exit(0);
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port <port>          Server port (default: 7777)" << std::endl;
    std::cout << "  --log <file>           Log file path (default: simpledb.log)" << std::endl;
    std::cout << "  --role <leader|follower>  Replication role (default: leader)" << std::endl;
    std::cout << "  --leader <host:port>   Leader address (for follower role)" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int port = 7777;
    std::string log_file = "simpledb.log";
    std::string role_str = "leader";
    std::string leader_addr = "";
    
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_file = argv[++i];
        } else if (std::strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            role_str = argv[++i];
        } else if (std::strcmp(argv[i], "--leader") == 0 && i + 1 < argc) {
            leader_addr = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "SimpleDB - A simple key-value database" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  - Concurrent connections" << std::endl;
    std::cout << "  - ACID compliance" << std::endl;
    std::cout << "  - R-tree indexing" << std::endl;
    std::cout << "  - Append-only log" << std::endl;
    std::cout << "  - Leader-follower replication" << std::endl;
    std::cout << "  - No external dependencies" << std::endl;
    std::cout << "=======================================" << std::endl << std::endl;
    
    // Create storage layer
    auto store = std::make_shared<storage::KVStore>(log_file);
    std::cout << "Storage initialized (log: " << log_file << ")" << std::endl;
    
    // Create transaction manager
    auto txn_mgr = std::make_shared<transaction::TransactionManager>(store);
    std::cout << "Transaction manager initialized" << std::endl;
    
    // Create replication layer
    replication::ReplicatorRole role = replication::ReplicatorRole::LEADER;
    if (role_str == "follower") {
        role = replication::ReplicatorRole::FOLLOWER;
    }
    
    auto replicator = std::make_unique<replication::Replicator>(log_file, role);
    
    if (role == replication::ReplicatorRole::FOLLOWER && !leader_addr.empty()) {
        // Parse leader address
        size_t colon_pos = leader_addr.find(':');
        if (colon_pos != std::string::npos) {
            std::string leader_host = leader_addr.substr(0, colon_pos);
            int leader_port = std::atoi(leader_addr.substr(colon_pos + 1).c_str());
            replicator->connect_to_leader(leader_host, leader_port);
            std::cout << "Connected to leader at " << leader_addr << std::endl;
        }
    }
    
    replicator->start();
    std::cout << "Replication started (role: " << role_str << ")" << std::endl;
    
    // Create and start server
    g_server = std::make_unique<network::Server>(port, txn_mgr);
    std::cout << "Starting server on port " << port << "..." << std::endl;
    
    g_server->start();
    
    return 0;
}
