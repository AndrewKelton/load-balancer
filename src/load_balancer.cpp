#include "load_balancer.hpp"
#include "thread_pool.hpp"

#include <cstring>
#include <thread>
#include <functional>
#include <iomanip>

/* Next steps:
 * (2) Merge round_robin_server_delegator() and least_connections_server_delegator()
 * into one function. The only real difference is rr takes client_fd and current server idx,
 * while lc takes only client_fd. Also rr calls get_next_server_index_round_robin(), while lc calls
 * get_server_index_with_least_connections().
 */

bool load_balancer::LoadBalancer::setup_server(int port) {
    
    load_balancer_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (load_balancer_fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    // set reuse address to true
    int optval = 1;
    if (setsockopt(load_balancer_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Error setting reuse socket" << std::endl;
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;

    // set timeout for receiving data
    if (setsockopt(load_balancer_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        return false;
    }

    // set timeout for sending data
    if (setsockopt(load_balancer_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting send timeout" << std::endl;
        return false;
    }
    
    address = create_address(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(load_balancer_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
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

        // send health check
        int bytes_sent = send(server_socket, load_balancer::health_endpoint, strlen(load_balancer::health_endpoint) + 1, 0); 
        
        char buffer[SMALL_BUFFER_SIZE] = {0};

        // receive response from backend
        int received_status = recv(server_socket, buffer, SMALL_BUFFER_SIZE, 0);

        std::string_view buffer_view = buffer;
        if (buffer_view.find("200") == std::string_view::npos) {
            server.status.store(ServerHealthStatus::FAILED);
        } else {
            server.status.store(ServerHealthStatus::OK); // ensures response was actually successful
        }
    }

    close(server_socket); // close server socket
}

void load_balancer::LoadBalancer::health_checker(volatile std::sig_atomic_t& g_running) {

    while (g_running) {
        for (size_t i = 0; i < MAX_NUM_SERVERS; ++i) {
            send_health_check(servers.at(i));
        }

        std::this_thread::sleep_for(std::chrono::seconds(HEALTH_CHECK_INTERVAL_S));
    }
}

void load_balancer::LoadBalancer::send_request_recv_response(int backend_fd, std::string client_buffer, std::string& response) {
    
    // send message to backend
    ssize_t bytes_sent = send(backend_fd, client_buffer.c_str(), client_buffer.size(), 0); 
    
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

int load_balancer::LoadBalancer::get_next_server_index_round_robin(int server_idx) {

    for (int i = 0; i < MAX_NUM_SERVERS; ++i) {
        int curr_idx = (server_idx + i) % MAX_NUM_SERVERS;

        if (servers[curr_idx].status.load() == ServerHealthStatus::OK) {
            return curr_idx;
        }
    }

    return -1;
}

void load_balancer::LoadBalancer::round_robin_server_delegator(int client_fd, int curent_server_idx) {

    std::string full_client_buffer;
    char client_buffer[LARGE_BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, client_buffer, LARGE_BUFFER_SIZE);

    if (bytes_read <= 0) { // nothing read
        close(client_fd);
        return;
    }

    full_client_buffer.append(client_buffer, bytes_read);

    std::string response;
    int chosen_server_idx = get_next_server_index_round_robin(curent_server_idx);

    if (chosen_server_idx < 0) {
        // no servers are active
        std::cerr << "No servers available." << std::endl;
        response = "HTTP/1.1 503 Service Unavailable\r\n";
    } else {

        servers[chosen_server_idx].active_connections.fetch_add(1, std::memory_order_relaxed);

        int backend_fd = socket(AF_INET, SOCK_STREAM, 0);

        // some server is active
        sockaddr_in server_address = create_address(servers[chosen_server_idx].port);
        
        int connection_status = connect(backend_fd, (struct sockaddr*)&server_address, sizeof(server_address));
        
        // fallback, try to find a server that will connect with round robin
        bool close_connection = true;
        int retries = 1;
        while (retries < MAX_NUM_SERVERS && connection_status < 0) {
            servers[chosen_server_idx].active_connections.fetch_sub(1, std::memory_order_relaxed);
            close(backend_fd);
            ++retries;

            chosen_server_idx = get_next_server_index_round_robin(chosen_server_idx + 1);
            if (chosen_server_idx < 0) {
                close_connection = false;
                break; // no servers are active now
            }

            servers[chosen_server_idx].active_connections.fetch_add(1, std::memory_order_relaxed);
            backend_fd = socket(AF_INET, SOCK_STREAM, 0);
            server_address = create_address(servers[chosen_server_idx].port);
            connection_status = connect(backend_fd, (struct sockaddr*)&server_address, sizeof(server_address));
        }

        if (connection_status < 0) {
            std::cerr << "Could not connect to backend" << std::endl;
            response = 
                "HTTP/1.1 503 Service Unavailable\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 20\r\n"
                "No Backend Available"
            ;
        } else {

            // send request and receive response to/from backend
            send_request_recv_response(backend_fd, full_client_buffer, response);

            servers[chosen_server_idx].active_connections.fetch_sub(1, std::memory_order_relaxed);
        }

        if (close_connection) {
            close(backend_fd); // close backend socket
        }
    }
    write(client_fd, response.c_str(), response.size());
    close(client_fd); // close client socket
}

int load_balancer::LoadBalancer::get_server_index_with_least_connections(std::set<int> server_indexes_tried) {
    
    int index = -1;
    int least_connections = INT_MAX;

    for (int i = 0; i < MAX_NUM_SERVERS; ++i) {

        if (server_indexes_tried.contains(i)) {
            continue; // skip servers that have already tried connecting to
        }

        int current_load = servers[i].active_connections.load(std::memory_order_relaxed);
        ServerHealthStatus status = servers[i].status.load(std::memory_order_relaxed);

        if (current_load < least_connections && status == ServerHealthStatus::OK) {
            least_connections = current_load;
            index = i;
        }
    }

    return index;
}

void load_balancer::LoadBalancer::least_connections_server_delegator(int client_fd) {

    std::string full_client_buffer;
    char client_buffer[LARGE_BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, client_buffer, LARGE_BUFFER_SIZE);
    
    if (bytes_read <= 0) { // nothing read
        close(client_fd);
        return;
    }

    full_client_buffer.append(client_buffer, bytes_read); // need to change this to a loop later?

    std::string response;
    std::set<int> server_indexes_tried;

    int chosen_server_idx = get_server_index_with_least_connections(server_indexes_tried);

    if (chosen_server_idx < 0) { 
        // no servers are active
        std::cerr << "No servers available." << std::endl;
        response = "HTTP/1.1 503 Service Unavailable\r\n";
    } else {

        servers[chosen_server_idx].active_connections.fetch_add(1, std::memory_order_relaxed);

        int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
        
        // some server is active and it has the least connections
        sockaddr_in server_address = create_address(servers[chosen_server_idx].port);
        int connection_status = connect(backend_fd, (struct sockaddr*)&server_address, sizeof(server_address));
        
        bool close_connection = true;
        while (server_indexes_tried.size() < MAX_NUM_SERVERS && connection_status < 0) {
            servers[chosen_server_idx].active_connections.fetch_sub(1, std::memory_order_relaxed);
            server_indexes_tried.insert(chosen_server_idx); // insert the chosen server to the set
            close(backend_fd);
            
            chosen_server_idx = get_server_index_with_least_connections(server_indexes_tried);
            if (chosen_server_idx < 0) {
                close_connection = false;
                break; // no servers are active now
            }

            servers[chosen_server_idx].active_connections.fetch_add(1, std::memory_order_relaxed);
            backend_fd = socket(AF_INET, SOCK_STREAM, 0);
            server_address = create_address(servers[chosen_server_idx].port);
            connection_status = connect(backend_fd, (struct sockaddr*)&server_address, sizeof(server_address));
        }

        if (connection_status < 0) {
            std::cerr << "Could not connect to backend." << std::endl;
            response = 
                "HTTP/1.1 503 Service Unavailable\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 20\r\n"
                "No Backend Available"
            ;
        } else {

            // send request and receive response to/from backend
            send_request_recv_response(backend_fd, full_client_buffer, response); 

            servers[chosen_server_idx].active_connections.fetch_sub(1, std::memory_order_relaxed);
        }

        if (close_connection) {
            close(backend_fd); // close backend socket
        }
    }

    write(client_fd, response.c_str(), response.size());
    close(client_fd); // close client socket
}

std::string load_balancer::server_health_status_to_string(load_balancer::ServerHealthStatus status) {
    switch (status) {
        case load_balancer::ServerHealthStatus::OK:
            return "OK";
        case load_balancer::ServerHealthStatus::DEGRADED:
            return "DEGRADED";
        case load_balancer::ServerHealthStatus::FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

void load_balancer::LoadBalancer::print_load_balancer_info() {

    constexpr int PORT_WIDTH = 14;
    constexpr int HEALTH_WIDTH = 14;
    constexpr int CONNECTION_WIDTH = 22;

    std::cout << "\n========== Load Balancer Info ==========\n";
    std::cout << "Loading Method: " << load_method << "\n\n";
    if (load_method == "round-robin") {
        int next_server_idx = get_next_server_index_round_robin(current_server);
        std::cout << "Next Server: " << (next_server_idx >= 0 ? std::to_string(servers[next_server_idx].port) : "null") << "\n\n";
    }

    std::cout << std::left
              << "+" << std::string(PORT_WIDTH, '-') 
              << "+" << std::string(HEALTH_WIDTH, '-')
              << "+" << std::string(CONNECTION_WIDTH, '-') << "+\n";

    std::cout << "| " << std::setw(PORT_WIDTH - 1) << "Server Port"
              << "| " << std::setw(HEALTH_WIDTH - 1) << "Health"
              << "| " << std::setw(CONNECTION_WIDTH - 1) << "Active Connections"
              << "|\n";

    std::cout << "+" << std::string(PORT_WIDTH, '-')
              << "+" << std::string(HEALTH_WIDTH, '-')
              << "+" << std::string(CONNECTION_WIDTH, '-')
              << "+\n";

    for (const auto& server : servers) {
        std::cout << "| " << std::setw(PORT_WIDTH - 1) << server.port
                  << "| " << std::setw(HEALTH_WIDTH - 1)
                  << server_health_status_to_string(server.status.load())
                  << "| " << std::setw(CONNECTION_WIDTH - 1)
                  << server.active_connections.load()
                  << "|\n";
    }

    std::cout << "+"
              << std::string(PORT_WIDTH, '-')
              << "+"
              << std::string(HEALTH_WIDTH , '-')
              << "+"
              << std::string(CONNECTION_WIDTH, '-')
              << "+\n\n";
}

void load_balancer::LoadBalancer::print_load_balancer_info_worker(volatile std::sig_atomic_t& g_running) {

    while (g_running) {
        print_load_balancer_info();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool load_balancer::LoadBalancer::start_listen(volatile std::sig_atomic_t& g_running) {

    // start listening for incoming connections
    if (listen(load_balancer_fd, BACKLOG_SIZE) < 0) {
        std::cerr << "Error listening for incoming connections" << std::endl;
        return false;
    }

    std::cout << "Load Balancer listening on port " << port << std::endl;

    // create health check worker
    std::thread health_worker([this, &g_running]() {
        health_checker(g_running);
    });

    std::thread printer_worker([this, &g_running]() {
        print_load_balancer_info_worker(g_running);
    });
    
    // create worker threads
    ThreadPool pool;

    // pick load balancing strategy once before accept runs
    std::function<void(int)> load_method_function;
    if (load_method == "round-robin") {
        // round robin chosen
        load_method_function = [this, &pool](int client_fd) {
            int current_server_idx = current_server.load();

            pool.enqueue([this, client_fd, current_server_idx]() {
                round_robin_server_delegator(client_fd, current_server_idx);
            });

             if (current_server.load() < MAX_NUM_SERVERS - 1) {
                current_server.fetch_add(1);
            } else {
                current_server.store(0);
            }
        };

    } else {
        // ignore all else, just default to least-connections
        load_method_function = [this, &pool](int client_fd) {
            pool.enqueue([this, client_fd]() {
                least_connections_server_delegator(client_fd);
            });
        };
    }
    
    std::cout << pool.num_threads << " server delegators are working." << std::endl << std::endl;;
    
    while (g_running) {

        int address_len = sizeof(address); // get address length

        int client_fd = accept(load_balancer_fd,  (struct sockaddr*)&address, (socklen_t*)&address_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            continue;
        }

        load_method_function(client_fd);
    }

    health_worker.join();    // stop the health check worker
    printer_worker.join();   // stop the print info worker
    close(load_balancer_fd); // close load balancer socket

    std::cout << std::endl << "Load Balancer on port " << port << " has closed its connection." << std::endl;

    return true;
}