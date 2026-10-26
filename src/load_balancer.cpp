#include "load_balancer.hpp"
#include "utils.hpp"

#include <cstring>

bool load_balancer::LoadBalancer::setup_server(int port) {
    
    this->load_balancer_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (load_balancer_fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    // set reuse address to true
    int optval = 1;
    if (setsockopt(this->load_balancer_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting reuse socket" << std::endl;
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    // set timeout for receiving data
    if (setsockopt(this->load_balancer_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        return false;
    }

    // set timeout for sending data
    if (setsockopt(this->load_balancer_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting send timeout" << std::endl;
        return false;
    }
    
    this->address = create_address(port);
    this->address.sin_addr.s_addr = INADDR_ANY;

    if (bind(this->load_balancer_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error binding socket with IP and port" << std::endl;
        return false;
    }
    return true;
}

sockaddr_in load_balancer::LoadBalancer::create_address(int server_port) {

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(server_port);

    return address;
}

void load_balancer::LoadBalancer::send_health_check(server_info_t& server) {

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address = create_address(server.port);

    if (connect(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        // std::cerr << "Could not connect to server on port " << server.port << std::endl;
        
        if (server.failure_count < MAX_FAILURE_COUNT) {
            ++server.failure_count;
        }

        if (server.failure_count > 2) {
            server.status.store(ServerHealthStatus::FAILED);
        } else {
            server.status.store(ServerHealthStatus::DEGRADED);
        }

    } else {
        server.failure_count = 0;
        server.status.store(ServerHealthStatus::OK);

        // send health check
        int bytes_sent = send(server_socket, load_balancer::health_endpoint, strlen(load_balancer::health_endpoint) + 1, 0); 
        
        char buffer[SMALL_BUFFER_SIZE] = {0};

        // receive response from backend
        int received_status = recv(server_socket, buffer, SMALL_BUFFER_SIZE, 0);

        std::string_view buffer_view = buffer;
        if (buffer_view.find("200") == std::string_view::npos) {
            server.status.store(ServerHealthStatus::FAILED);
        }
    }

    close(server_socket); // close server socket
}

void load_balancer::LoadBalancer::health_checker(volatile std::sig_atomic_t& g_running) {
    while (g_running) {
        for (int i = 0; i < this->servers.size(); ++i) {
            send_health_check(this->servers.at(i));
        }

        std::this_thread::sleep_for(std::chrono::seconds(HEALTH_CHECK_INTERVAL_S));
    }
}

bool load_balancer::LoadBalancer::get_next_healthy_server() {
    int count = 0; 
    int max_servers = this->servers.size();

    while (count < max_servers) {
        if (this->servers.at(this->current_server).status.load() == ServerHealthStatus::FAILED || this->servers.at(this->current_server).status.load() == ServerHealthStatus::DEGRADED) {
            ++count;
            
            if (this->current_server >= max_servers) {
                this->current_server = 0;
            } else {
                ++this->current_server;
            }
            
        } else {
            return true; // active healthy server found
        }
    }

    // no active server found
    this->current_server = -1; 
    return false;
}

bool load_balancer::LoadBalancer::start_listen(volatile std::sig_atomic_t& g_running) {

    // start listening for incoming connections
    if (listen(this->load_balancer_fd, 3) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return false;
    }

    std::cout << "Load Balancer listening on port " << this->port << std::endl;

    // create health check worker
    std::thread health_worker([this, &g_running]() {
        this->health_checker(g_running);
    });
    
    while (g_running) {

        int address_len = sizeof(this->address); // get address length

        int client_fd = accept(this->load_balancer_fd,  (struct sockaddr*)&this->address, (socklen_t*)&address_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            continue;
        }

        std::cout << "Received request from client" << std::endl;

        std::string full_client_buffer;
        char client_buffer[LARGE_BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_fd, client_buffer, LARGE_BUFFER_SIZE);

        if (bytes_read <= 0) { // nothing read
            close(client_fd);
            continue;
        }

        /* Have to do this differently for read(), need to read until blank
         * line "\r\n\r\n" is received. 
         */
        // while (bytes_read > 0) { 
        full_client_buffer.append(client_buffer, bytes_read);
        // bytes_read = read(client_fd, client_buffer, LARGE_BUFFER_SIZE);
        // }

        int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
        std::string response;
        
        if (!get_next_healthy_server() || this->current_server < 0) {
            // no servers are active
            std::cerr << "No servers available." << std::endl;
            response = "HTTP/1.1 503 Service Unavailable\r\n";
        } else {
            // create address for backend
            sockaddr_in server_address = create_address(servers[current_server].port);
            server_address.sin_port = htons(servers[current_server].port);
            
            // connect to the backend
            if (connect(backend_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
                std::cerr << "Could not connect to backend server on port " << servers[current_server].port << std::endl;
                response = 
                    "HTTP/1.1 503 Service Unavailable\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 20\r\n"
                    "No Backend Available"
                    ;
            } else {
                std::cout << "Connected to: " << std::endl << "IP: " << server_address.sin_family << std::endl << "Port: " << servers.at(current_server).port << std::endl;
                
                // send message to backend
                ssize_t bytes_sent = send(backend_fd, full_client_buffer.c_str(), full_client_buffer.size(), 0); 
                
                // receive response from backend
                char server_buffer[LARGE_BUFFER_SIZE] = {0};
                ssize_t bytes_received = recv(backend_fd, server_buffer, LARGE_BUFFER_SIZE, 0);

                if (bytes_received < 0) {
                    std::cerr << "Error receiving data from backend" << std::endl;
                    response = "HTTP/1.1 502 Bad Gateway\r\n";
                } else if (bytes_received == 0) {
                    std::cerr << "Remote connection has closed its connection!" << std::endl;
                    response = "HTTP/1.1 502 Bad Gateway\r\n";
                } else {

                    // receive all bytes until backend closes its connection
                    while (bytes_received > 0) {
                        response.append(server_buffer, bytes_received);
                        bytes_received = recv(backend_fd, server_buffer, LARGE_BUFFER_SIZE, 0);
                    }
                }
            }
        }

        close(backend_fd); // close backend socket
        write(client_fd, response.c_str(), response.size());
        close(client_fd); // close client socket
        
        if (this->current_server < this->servers.size() - 1) {
            ++this->current_server;
        } else {
            this->current_server = 0;
        }
    }

    health_worker.join(); // stop the health check worker
    close(this->load_balancer_fd); // close socket

    return true;
}