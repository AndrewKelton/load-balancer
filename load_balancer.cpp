#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <vector>

#define MAX_NUM_SERVERS 3
#define HEALTHY 1
#define IFFY 2
#define DED 3

using health_status = int;

struct ServerTracker {
    int port;
    int failure_count = 0;
    health_status status = HEALTHY; // 1 == healthy, 2 == iffy, 3 == ded
};
using server_tracker_t = ServerTracker;

sockaddr_in create_address(int port) {
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    return address;
}

void send_health_checks(std::vector<server_tracker_t>& servers) {

    std::cout << "Health Checks:" << std::endl;
    const char * health_check = "GET /health HTTP/1.1";
    
    for (int i = 0; i < servers.size(); i++) {

        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address = create_address(servers.at(i).port);

        // connect to the server
        if (connect(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Could not connect to backend server on port " << servers.at(i).port << std::endl;
            servers.at(i).failure_count++;
            
            if (servers.at(i).failure_count > 2) {
                servers.at(i).status = DED;
                std::cout << "Port: " << servers.at(i).port << std::endl << "Status: DED" << std::endl << std::endl;
            } else {
                servers.at(i).status = IFFY;
                i--;
            }
            continue;
        } else {
            if (servers.at(i).failure_count > 0) {
                servers.at(i).failure_count--;
                servers.at(i).status = IFFY;
            } else {
                servers.at(i).status = HEALTHY;
            }
        }
        // send health check
        int bytes_sent = send(server_socket, health_check, 1024, 0); 
        
        char buffer[1024] = {0};
        // receive response from backend
        int received_status = recv(server_socket, buffer, 1024, 0);

        std::string buffer_view = buffer;
        if (buffer_view.find("200") == std::string_view::npos) {
            servers.at(i).status = DED;
        } 

        std::cout << "Port: " << servers.at(i).port << std::endl << "Status: " << servers.at(i).status << std::endl << std::endl;
    }
}

int main() {

    std::vector<server_tracker_t> servers(MAX_NUM_SERVERS);
    servers[0].port = 9001;
    servers[1].port = 9002;
    servers[2].port = 9003;

    int port = 8080;
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

    // 2. Bind the socket to local IP and port 8080
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        strerror(errno);
        std::cerr << "Error binding socket with IP and port" << std::endl;
        return 1; 
    }

    // 3. Start listening for incoming connections
    if (listen(server_socket, 0) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return 1;
    }

    std::cout << "Load balancer listening on port " << port << std::endl;

    int current_server = 0;

    // 4. Accept a connection loop
    while (true) {

        send_health_checks(servers); // check server healths

        int address_len = sizeof(address);
        int client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&address_len);

        if (client_socket < 0) {
            continue;
        }
        std::cout << "Received request from client" << std::endl;

        char client_buffer[1024];

        // Read incoming request data
        read(client_socket, client_buffer, 1024);

        // set server back to bottom of list
        if (current_server == MAX_NUM_SERVERS) {
            current_server = 0; 
        }

        // create socket for backend connection
        int backend_socket = socket(AF_INET, SOCK_STREAM, 0);

        // continue to next server in list if current server is unhealthy
        while (servers[current_server].status == DED) {
            current_server++;
            if (current_server >= MAX_NUM_SERVERS) { // cannot exceed max number of servers
                current_server = 0;
            }
        }

        // create address for backend
        sockaddr_in server_address = create_address(servers[current_server].port);
        server_address.sin_port = htons(servers[current_server].port);

        // connect to the backend
        if (connect(backend_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
            std::cerr << "Could not connect to backend server on port " << servers[current_server].port << std::endl;
            current_server++;
            continue;
        }
        std::cout << "Connected to: " << std::endl << "IP: " << server_address.sin_family << std::endl << "Port: " << servers.at(current_server).port << std::endl;

        std::string message = "hello " + std::to_string(servers[current_server].port);

        // send message to backend
        int bytes_sent = send(backend_socket, client_buffer, 1024, 0); 

        char buffer[1024] = {0};

        // receive response from backend
        int received_status = recv(backend_socket, buffer, 1024, 0);
        
        if (received_status < 0) {
            std::cerr << "Error receiving data from backend" << std::endl;
            continue;
        } else if (received_status == 0) {
            std::cerr << "Remote connection has closed its connection!" << std::endl;
            continue;
        }

        std::string buffer_string = buffer;
        std::string http_response = buffer;

        write(client_socket, http_response.c_str(), http_response.length());
        close(client_socket);

        current_server++;
    }

    close(server_socket);
}