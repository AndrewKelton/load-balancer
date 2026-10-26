#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>

#include "utils.hpp"

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

    int port = std::stoi(argv[1]);
    std::string port_string = argv[1];

    // 1. Create Socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }

    // set SO_REUSEADDR on the socket to true
    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting socket options" << std::endl;
        return 1;
    }

    // 2. Bind the socket to local IP and port 900X
    sockaddr_in address = {};
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // listen on all network interfaces
    address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error binding socket with IP and port" << std::endl;
        return 1;
    }

    // 3. Start listening for incoming connections
    if (listen(server_socket, 0) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    // 4. Accept a connection loop
    while (true) {
        int address_len = sizeof(address);
        int client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&address_len);

        if (client_socket < 0) {
            continue;
        }

        // Read incoming request data
        std::string full_buffer;
        char buffer[LARGE_BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_socket, buffer, LARGE_BUFFER_SIZE);

        if (bytes_read <= 0) { // do nothing
            close(client_socket);
            continue;
        }

        /* Have to do this differently for read(), need to read until blank
         * line "\r\n\r\n" is received. 
         */
        // while (bytes_read > 0) {
        full_buffer.append(buffer, bytes_read);
        //     bytes_read = read(client_socket, buffer, LARGE_BUFFER_SIZE);
        // }

        std::cout << "Received Request: " << std::endl << buffer << std::endl;

        std::string response; 
        request_t request = parse_request(buffer); // parse the request
        if (request.method.empty()) {
            response = "HTTP/1.1 400 Bad Request"; // request was malformed
        } else {
            response = get_response(request, port_string); // get the response
        }

        write(client_socket, response.c_str(), response.size());
        close(client_socket); // close connection for this request
    }

    close(server_socket); // close server socket

    return 0;
}