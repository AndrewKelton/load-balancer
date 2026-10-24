#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#define MAX_NUM_SERVERS 3
#define MAX_FAILURE_COUNT 5
#define HEALTHY 1
#define IFFY 2
#define DED 3

const char * health_check = "GET /health HTTP/1.1";

using health_status = int;

struct ServerTracker {
    int port; // 9001, 9002, ...
    int failure_count = 0; // # of times this server has consecutively failed health checks
    std::atomic<health_status> status = HEALTHY; // 1 == healthy, 2 == iffy, 3 == ded
};
using server_tracker_t = ServerTracker;

/* Creates address for a socket */
sockaddr_in create_address(int port) {

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    return address;
}

/* Connects to the server specified and sends a health check using 
 * the /health endpoint. If a server returns 200 OK, then that server
 * is considered HEALTHY, if it returns anything else, then it is DED.
 */
bool send_health_check(server_tracker_t& server) {

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address = create_address(server.port);

    if (connect(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Could not connect to server on port " << server.port << std::endl;
        
        if (server.failure_count < MAX_FAILURE_COUNT) {
            ++server.failure_count;
        }

        if (server.failure_count > 2) {
            server.status.store(DED);
        } else {
            server.status.store(IFFY);
        }

        return false; // return unsuccessful health check
    }

    if (server.failure_count > 0) {
        --server.failure_count;
        server.status.store(IFFY);
    } else {
        server.status.store(HEALTHY);
    }

    // send health check
    int bytes_sent = send(server_socket, health_check, 1024, 0); 
    
    char buffer[1024] = {0};
    // receive response from backend
    int received_status = recv(server_socket, buffer, 1024, 0);

    std::string buffer_view = buffer;
    if (buffer_view.find("200") == std::string_view::npos) {
       server.status.store(DED);

       return false; // return unsuccessful health check
    } 

    return true; // return successful health check
}

/* Wrapper function to send health checks to all servers in the list of servers*/
void send_health_checks(std::vector<server_tracker_t>& servers) {

    std::cout << "Health Checks:" << std::endl;
    
    for (int i = 0; i < servers.size(); i++) {
        send_health_check(servers.at(i));
        std::cout << "Port: " << servers.at(i).port << std::endl << "Status: " << servers.at(i).status << std::endl << std::endl;
    }
}

/* worker to send health checks to the servers every 2 seconds */
void health_check_worker(std::vector<server_tracker_t>& servers) {
    while (true) {
        send_health_checks(servers);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

/* Locates and returns the index of the next healthy server in the list of servers.
 * If no healthy server is found in the list of servers, returns -1.
 */
int get_next_healthy_server(int initial_server, std::vector<server_tracker_t>& servers) {

    int current_server = initial_server;
    int count = 0;

    while (servers.at(current_server).status.load() == DED || servers.at(current_server).status.load() == IFFY) {
        current_server++;
        count++;
        if (current_server >= MAX_NUM_SERVERS) { // cannot exceed max number of servers
            current_server = 0;
        }

        if (count > MAX_NUM_SERVERS) { // get out of inf loop
            break;
        }
    }

    if (count > MAX_NUM_SERVERS) {
        return -1;
    }

    return current_server;
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
        std::cerr << "Error setting reuse socket" << std::endl;
        return 1;
    }

    // set timeout for receiving data
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
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

    // Create health check worker
    std::thread health_worker([&servers]() {
        health_check_worker(std::ref(servers));
    });

    // 4. Accept a connection loop
    while (true) {

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

        // get the next healthy server
        current_server = get_next_healthy_server(current_server, servers);

        std::string response;

        if (current_server < 0) {
            // no server is healthy
            std::cerr << "No servers available" << std::endl;
            response = "HTTP/1.1 503 Service Unavailable\r\n";
        } else {

            // create address for backend
            sockaddr_in server_address = create_address(servers[current_server].port);
            server_address.sin_port = htons(servers[current_server].port);

            char server_buffer[1024] = {0}; // 

            // connect to the backend
            if (connect(backend_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
                std::cerr << "Could not connect to backend server on port " << servers[current_server].port << std::endl;
                response = "HTTP/1.1 503 Service Unavailable\r\n";
            } else {
                std::cout << "Connected to: " << std::endl << "IP: " << server_address.sin_family << std::endl << "Port: " << servers.at(current_server).port << std::endl;

                // send message to backend
                int bytes_sent = send(backend_socket, client_buffer, 1024, 0); 

                // receive response from backend
                int received_status = recv(backend_socket, server_buffer, 1024, 0);
                
                if (received_status < 0) {
                    std::cerr << "Error receiving data from backend" << std::endl;
                    response = "HTTP/1.1 502 Bad Gateway\r\n";
                } else if (received_status == 0) {
                    std::cerr << "Remote connection has closed its connection!" << std::endl;
                    response = "HTTP/1.1 502 Bad Gateway\r\n";
                } else {
                    response = server_buffer;
                }
            }
        }

        write(client_socket, response.c_str(), response.length());
        close(client_socket);

        current_server++;
    }

    health_worker.join(); // stop the health check worker

    close(server_socket); // close socket
}