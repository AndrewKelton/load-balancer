#include "server.hpp"
#include "utils.hpp"

#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>

/* Need to refactor Server with a dedicated RAII socket class to
 * automatically close file descriptor. Replace int server_fd_ w/
 * UniqueSocket class for immediate cleanup of socket.
 */

int Server::setup_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }

    // set reuse address to true
    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting reuse socket" << std::endl;
        close (fd);
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    // set timeout for receiving data
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        close (fd);
        return -1;
    }

    // set timeout for sending data
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting send timeout" << std::endl;
        close (fd);
        return -1;
    }

    return fd;
}

bool Server::setup_server() {

    server_fd_ = setup_socket();
    
    std::optional<sockaddr_in> address = create_address(port_);
    if (address == std::nullopt) {
        std::cerr << "Error creating address with port " << port_ << std::endl;
        close(server_fd_);
        return false;
    }

    address_ = address.value();
    address_.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd_, (struct sockaddr*)&address_, sizeof(address_)) < 0) {
        std::cerr << "Error binding socket with IP and port" << std::endl;
        close(server_fd_);
        return false;
    }

    return true;
}

bool Server::start_listen() {
    if (listen(server_fd_, BACKLOG_SIZE) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return false;
    }
    is_listening = true;

    std::cout << "Server listening on port: " << port_ << std::endl;

    return true;
}

std::pair<int, sockaddr_in> Server::accept_connection() {
    
    sockaddr_in client_address{};
    socklen_t client_len = sizeof(client_address);

    int client_fd = accept(server_fd_, (struct sockaddr*)&client_address, &client_len);
    if (client_fd < 0) {
        if (errno == EINTR) {
            return {-2, client_address};
        }
        return {-2, client_address};
    }

    return {client_fd, client_address};
}

int Server::get_port() {
    return port_;
}

int Server::get_server_fd() {
    return server_fd_;
}

bool Server::server_is_listening() {
    return is_listening;
}
        
sockaddr_in Server::get_address() {
    return address_;
}

void Server::set_port(int port) {
    port_ = port;
}

ssize_t Server::read_request(int client_fd, std::string& full_client_buffer) {
    
    std::string tmp_buffer;
    ssize_t total_bytes_read = 0;
    size_t content_length = 0;
    size_t content_bytes_read = 0;

    /* This now works and waits for the entire request from the user. However,
     * adding some sort of a max bytes read may be necessary. For now this works.
     */
    while (tmp_buffer.find("\r\n\r\n") == std::string::npos || (content_length > 0 && content_bytes_read < content_length)) {
        char client_buffer[LARGE_BUFFER_SIZE] = {0};
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

        tmp_buffer.append(client_buffer, bytes_read);
        total_bytes_read += bytes_read; 
        
        /* This can honestly be done using parse_reqest from utils.hpp 
         * but I just did this to get it working for now. Later I need
         * to refactor the server to use the parse_request function.
         */
        if (content_length == 0) {
            // parse the content-length
            std::string local_tmp_buffer = client_buffer;
            if (local_tmp_buffer.find("Content-Length") != std::string::npos) {
                std::vector<std::string_view> content_split = split(local_tmp_buffer, ":");
                content_length = to_size_t(trim(content_split[1])).value();
            }
        }

        // ensures that we read all the bytes in Content-Length
        if (content_length > 0 && tmp_buffer.find("\r\n\r\n") != std::string::npos) {
            content_bytes_read += bytes_read;
        }
    }

    full_client_buffer = tmp_buffer; // need to fix this later

    return total_bytes_read;
}

std::optional<sockaddr_in> create_address(int port) {

    if (port <= 0) {
        std::cerr << "Invalid port input, cannot create address." << std::endl;
        return std::nullopt;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    return address;
}

bool send_all(int fd, const char * data, size_t len) {

    while (len > 0) {
        ssize_t n = ::send(fd, data, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue; // interrupted, retry
            }
            return false; // peer gone, or some actual error
        }

        data += n;
        len -= static_cast<size_t>(n);
    }

    return true;
}