#include "load_balancer.hpp"

bool load_balancer::LoadBalancer::setup_server(int port) {
    
    this->socket_lb = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_lb < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    // set reuse address to true
    int optval = 1;
    if (setsockopt(this->socket_lb, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting reuse socket" << std::endl;
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    // set timeout for receiving data
    if (setsockopt(this->socket_lb, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        return false;
    }

    // set timeout for sending data
    if (setsockopt(this->socket_lb, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting send timeout" << std::endl;
        return false;
    }
    
    this->address = create_address(port);
    this->address.sin_addr.s_addr = INADDR_ANY;

    if (bind(this->socket_lb, (struct sockaddr*)&address, sizeof(address)) < 0) {
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
        std::cerr << "Could not connect to server on port " << server.port << std::endl;
        
        if (server.failure_count < MAX_FAILURE_COUNT) {
            ++server.failure_count;
        }

        if (server.failure_count > 2) {
            server.status.store(ServerHealthStatus::FAILED);
        } else {
            server.status.store(ServerHealthStatus::DEGRADED);
        }

        return; // return unsuccessful health check
    }

    if (server.failure_count > 0) {
        --server.failure_count;
        server.status.store(ServerHealthStatus::DEGRADED);
    } else {
        server.status.store(ServerHealthStatus::OK);
    }

    // send health check
    int bytes_sent = send(server_socket, load_balancer::health_endpoint, 1024, 0); 
    
    char buffer[1024] = {0};
    // receive response from backend
    int received_status = recv(server_socket, buffer, 22, 0);

    std::string buffer_view = buffer;
    if (buffer_view.find("200") == std::string_view::npos) {
       server.status.store(ServerHealthStatus::FAILED);
    }
}

void load_balancer::LoadBalancer::health_checker() {
    while (true) {
        for (int i = 0; i < this->servers.size(); ++i) {
            send_health_check(this->servers.at(i));
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

bool load_balancer::LoadBalancer::get_next_healthy_server() {
    int count = 0; 
    int max_servers = this->servers.size();

    while (this->servers.at(this->current_server).status.load() == ServerHealthStatus::FAILED || this->servers.at(this->current_server).status.load() == ServerHealthStatus::DEGRADED) {
        ++this->current_server;
        ++count;

        if (this->current_server >= max_servers) {
            this->current_server = 0; // go back to front
        }

        if (count > max_servers) {
            break;
        }
    }

    if (count > max_servers) {
        this->current_server = -1;
        return false;
    }

    return true;
}

bool load_balancer::LoadBalancer::start_listen() {

    // start listening for incoming connections
    if (listen(this->socket_lb, 3) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return false;
    }

    std::cout << "Load Balancer listening on port " << this->port << std::endl;

    // create health check worker
    std::thread health_worker([this]() {
        this->health_checker();
    });
    
    while (true) {

        int address_len = sizeof(this->address); // get address length

        int client_socket = accept(this->socket_lb,  (struct sockaddr*)&this->address, (socklen_t*)&address_len);
        if (client_socket < 0) {
            close(client_socket);
            continue;
        }

        std::cout << "Received request from client" << std::endl;

        char client_buffer[1024];
        read(client_socket, client_buffer, 1024);

        int backend_socket = socket(AF_INET, SOCK_STREAM, 0);
        get_next_healthy_server();

        std::string response;
        
        if (this->current_server < 0) {
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

            close(backend_socket); // close backend socket
        }

        write(client_socket, response.c_str(), response.size());
        close(client_socket); // close client socket
        
        ++this->current_server;
    }

    health_worker.join(); // stop the health check worker
    close(this->socket_lb); // close socket

    return true;
}