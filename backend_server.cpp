#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#define GET 1
#define POST 2
#define DELETE 3

using method_type = int;

struct HTTP_Request_Info {
    method_type method;
    std::string path_uri;
    std::string version;
};
using http_request_info_t = HTTP_Request_Info;

int decode_method(const std::string_view& meth) {
    if (meth == "GET") {
        return GET;
    } else if (meth == "POST") {
        return POST;
    } else if (meth == "DELETE") {
        return DELETE;
    }
    return -1;
}

http_request_info_t extract_from_request_line(const std::string_view& line) {

    http_request_info_t request;

    int method_end = line.find(' ');
    std::string_view meth = line.substr(0, method_end);
    
    int path_end = line.substr(method_end + 1).find(' ');
    
    request.method = decode_method(meth);
    request.path_uri = line.substr(method_end + 1, path_end);
    request.version = "HTTP/1.1"; // can ignore actual version for now

    return request;
}

std::string_view get_request_line(const std::string_view& buffer) {

    size_t end = buffer.find("\r\n");
    if (end == std::string_view::npos) { 
        return "";
    }

    return buffer.substr(0, end);
}

std::string get_response(http_request_info_t request_info, std::string port) {
    
    // what is our response for this endpoint
    if (request_info.path_uri == "/health") {
        return "HTTP/1.1 200 OK\r\n";
    } 
    return 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 21\r\n\r\n"
    "Hello from port: " + port;
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
        char buffer[1024] = {0};
        read(client_socket, buffer, 1024);
        std::cout << "Received Request: " << std::endl << buffer << std::endl;

        http_request_info_t request_info = extract_from_request_line(get_request_line(buffer));
        std::string http_response = get_response(request_info, port_string);

        write(client_socket, http_response.c_str(), http_response.length());
        close(client_socket); // close connection for this request
    }

    close(server_socket); // close server socket

    return 0;
}