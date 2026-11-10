#include "server.hpp"
#include "utils.hpp"

#include <iostream>
#include <unistd.h>

bool Server::setup_server() {

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd_ < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    // set reuse address to true
    int optval = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting reuse socket" << std::endl;
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    // set timeout for receiving data
    if (setsockopt(server_fd_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        return false;
    }

    // set timeout for sending data
    if (setsockopt(server_fd_, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting send timeout" << std::endl;
        return false;
    }
    
    address_ = create_address(port_);
    address_.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd_, (struct sockaddr*)&address_, sizeof(address_)) < 0) {
        std::cerr << "Error binding socket with IP and port" << std::endl;
        return false;
    }

    return true;
}

bool Server::start_listen() {
    if (listen(server_fd_, BACKLOG_SIZE) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return false;
    }

    std::cout << "Server listening on port: " << port_ << std::endl;

    return true;
}

int Server::accept_connection() {
    
    int address_len = sizeof(address_);

    int client_fd = accept(server_fd_, (struct sockaddr*)&address_, (socklen_t*)&address_len);
    if (client_fd < 0) {
        if (errno == EINTR) {
            return -2;
        }
        return -2;
    }

    return client_fd;
}

int Server::get_port() {
    return port_;
}

int Server::get_server_fd() {
    return server_fd_;
}
        
sockaddr_in Server::get_address() {
    return address_;
}

void Server::set_port(int port) {
    port_ = port;
}

sockaddr_in create_address(int port) {

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    return address;
}

ssize_t Server::read_request(int client_fd, std::string& full_client_buffer) {
    
    char client_buffer[LARGE_BUFFER_SIZE] = {0};
    ssize_t total_bytes_read = 0;
    
    
    while (total_bytes_read < sizeof(client_buffer) - 1) {
        ssize_t bytes_read = read(client_fd, client_buffer, LARGE_BUFFER_SIZE);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // non-blocking mode, wait for more data epoll?
            } else if (errno == EINTR) {
                continue; // interrupted by signal
            } else {
                break; // real error, disconnection
            }
        } else if (bytes_read == 0) {
            break; // connection closed
        }

        total_bytes_read += bytes_read;
    }

    client_buffer[total_bytes_read] = '\0';
    full_client_buffer = client_buffer; // need to fix this later

    return total_bytes_read;
}