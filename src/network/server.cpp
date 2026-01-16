#include "network/server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace simpledb {
namespace network {

Server::Server(int port, std::shared_ptr<transaction::TransactionManager> txn_mgr)
    : port_(port), server_fd_(-1), txn_mgr_(txn_mgr), running_(false) {
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (running_) {
        return;
    }
    
    server_fd_ = create_server_socket();
    if (server_fd_ < 0) {
        std::cerr << "Failed to create server socket" << std::endl;
        return;
    }
    
    running_ = true;
    
    std::cout << "SimpleDB server started on port " << port_ << std::endl;
    
    accept_connections();
}

void Server::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    // Wait for all worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

int Server::create_server_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        return -1;
    }
    
    // Bind to port
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(sockfd);
        return -1;
    }
    
    // Listen
    if (listen(sockfd, 10) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

void Server::accept_connections() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Error accepting connection" << std::endl;
            }
            continue;
        }
        
        // Handle connection in a new thread
        worker_threads_.emplace_back(&Server::handle_connection, this, client_fd);
    }
}

void Server::handle_connection(int client_fd) {
    Connection conn(client_fd, txn_mgr_);
    conn.handle();
}

} // namespace network
} // namespace simpledb
