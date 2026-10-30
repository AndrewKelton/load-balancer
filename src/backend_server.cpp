#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <csignal>

#include "utils.hpp"

static volatile std::sig_atomic_t g_running = 1;
static void on_sigint(int) { g_running = 0; }

std::string get_response(request_t request, std::string port) {
    
    // what is our response for this endpoint
    if (request.path_uri == "/health") {
        return "HTTP/1.1 200 OK\r\n"; // as of now just send healthy response
    } 
    std::string body = "Hello from port: " + port;
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
}

int main(int argc, char * argv[]) {

    if (argc < 2) {
        std::cerr << "No port specified" << std::endl;
        return 1;
    }

    // signal to stop server loop
    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART, so accept() returns EINTR
    sigaction(SIGINT, &sa, nullptr);

    int port = std::stoi(argv[1]);
    std::string port_string = argv[1];

    // 1. Create Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }

    // set SO_REUSEADDR on the socket to true
    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting socket options" << std::endl;
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    // set timeout for receiving data
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        return false;
    }

    // set timeout for sending data
    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting send timeout" << std::endl;
        return false;
    }

    // 2. Bind the socket to local IP and port 900X
    sockaddr_in address = {};
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // listen on all network interfaces
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error binding socket with IP and port" << std::endl;
        return 1;
    }

    // 3. Start listening for incoming connections
    if (listen(server_fd, BACKLOG_SIZE) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    // 4. Accept a connection loop
    while (g_running) {
        int address_len = sizeof(address);
        int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&address_len);

        if (client_fd < 0) {
            continue;
        }

        // Read incoming request data
        std::string full_buffer;
        char buffer[LARGE_BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_fd, buffer, LARGE_BUFFER_SIZE);

        if (bytes_read <= 0) { // do nothing
            close(client_fd);
            continue;
        }

        /* Have to do this differently for read(), need to read until blank
         * line "\r\n\r\n" is received. 
         */
        full_buffer.append(buffer, bytes_read);

        std::cout << "Received Request: " << std::endl << buffer << std::endl;

        std::string response; 
        request_t request = parse_request(buffer); // parse the request
        if (request.method.empty()) {
            response = "HTTP/1.1 400 Bad Request"; // request was malformed
        } else {
            response = get_response(request, port_string); // get the response
        }

        write(client_fd, response.c_str(), response.size());
        close(client_fd); // close connection for this request
    }

    close(server_fd); // close server socket

    std::cout << std::endl << "Server on port " << port << " has closed its connection." << std::endl;

    return 0;
}