#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

int main() {

    // 1. Create a socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0); // the socket
    if (server_socket < 0) { // check if creation succeeded
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }

    // set SO_REUSEADDR on the socket to true
    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting socket options" << std::endl;
        return 1;
    }

    // 2. Bind the socket to an IP and Port
    sockaddr_in address = {}; 
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // listen on all network interfaces
    address.sin_port = htons(8080); // listen on port 8080


    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error binding socket with IP and port" << std::endl;
        return 1;
    }

    // 3. Start listening for incoming connections
    if (listen(server_socket, 0) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return 1;
    }

    std::cout << "Server listening on port 8080" << std::endl;

    // 4. Accept a connection loop
    while (true) {
        int address_len = sizeof(address);
        int client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&address_len);

        if (client_socket < 0) {
            continue;
        }

        // Read incoming request data
        char buffer[1024] = {0};
        read(client_socket, buffer, 1024);
        std::cout << "Received Request: " << std::endl << buffer << std::endl;

        // Send a primitive HTTP response
        std::string http_response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n\r\n"
            "Hello";
        
        write(client_socket, http_response.c_str(), http_response.length());
        close(client_socket); // close connection for this request
    }

    close(server_socket); // close server socket
    
    return 0;
}