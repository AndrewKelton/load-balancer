#include "load_balancer.hpp"
#include "thread_pool.hpp"

#include <cstring>
#include <thread>
#include <functional>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <cstdint>

/* Next steps:
 * (2) Merge round_robin_server_delegator() and least_connections_server_delegator()
 * into one function. The only real difference is rr takes client_fd and current server idx,
 * while lc takes only client_fd. Also rr calls get_next_server_index_round_robin(), while lc calls
 * get_server_index_with_least_connections().
 */

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

void load_balancer::LoadBalancer::server_delegator(int client_fd, sockaddr_in client_address) {

    // check if request is allowed first
    if (!rate_limiter_.allow_request(convert_binary_ip_address_to_string(client_address))) {
        std::string response = 
            "HTTP/1.1 429 Too Many Requests\r\n"
            "Content-Type: text/plain\r\n"
            "Retry After: 60\r\n\r\n"; // change this to the actual retry after

        // request is not allowed, so tell client first 
        write(client_fd, response.c_str(), response.size());
        close(client_fd);

        return;
    }

    std::string client_buffer;
    if (read_request(client_fd, client_buffer) <= 0) {
        close(client_fd);
        return; // maybe setup in future for a retry?
    }

    std::string response;
    std::set<size_t> tried_server_indexes; // holds the indexes of servers tried to connect to
    std::optional<size_t> index = strategy->get_server_index(servers, tried_server_indexes);
    
    if (!index) {
        std::cerr << "No servers available." << std::endl;
        response = 
            "HTTP/1.1 503 Service Unavailable\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 20\r\n\r\n"
            "No Backend Available";
    } else {

        tried_server_indexes.insert(*index); // insert tried server

        servers[*index].active_connections.fetch_add(1, std::memory_order_relaxed);
        current_server.store(*index);

        int backend_fd = setup_socket();
        // some server is active
        sockaddr_in backend_address = create_address(servers[*index].port);

        int connection_status = connect(backend_fd, (struct sockaddr*)&backend_address, sizeof(backend_address));

        // fallback, try to find a server that will connect
        bool close_connection = true;
        int retries = 1;
        while (retries < servers.size() && connection_status < 0) {
            servers[*index].active_connections.fetch_sub(1, std::memory_order_relaxed);
            close(backend_fd);
            ++retries;

            index = strategy->get_server_index(servers, tried_server_indexes);
            if (!index) {
                close_connection = false;
                break; // no servers are active now
            }
            tried_server_indexes.insert(*index);
            current_server.store(*index);

            servers[*index].active_connections.fetch_add(1, std::memory_order_relaxed);
            backend_fd = setup_socket();
            backend_address = create_address(servers[*index].port);
            connection_status = connect(backend_fd, (struct sockaddr*)&backend_address, sizeof(backend_address));
        }

        if (connection_status < 0) {
            std::cerr << "Could not connect to backend" << std::endl;
            response = 
                "HTTP/1.1 503 Service Unavailable\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 20\r\n\r\n"
                "No Backend Available";
        } else {

            // send request and receive response to/from backend
            send_request_recv_response(backend_fd, client_buffer, response);
            servers[*index].active_connections.fetch_sub(1, std::memory_order_relaxed);
        }

        if (close_connection) {
            close(backend_fd);
        }

        write(client_fd, response.c_str(), response.size());
        close(client_fd);
    }
}

std::string load_balancer::server_health_status_to_string(ServerHealthStatus status) {
    switch (status) {
        case ServerHealthStatus::OK:
            return "OK";
        case ServerHealthStatus::DEGRADED:
            return "DEGRADED";
        case ServerHealthStatus::FAILED:
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

bool load_balancer::LoadBalancer::start_load_balancer(volatile std::sig_atomic_t& g_running) {

    // start listening for incoming connections
    start_listen();

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
    std::function<void(int, sockaddr_in)> delegator;
    delegator = [this, &pool](int client_fd, sockaddr_in client_address) {
        pool.enqueue([this, client_fd, client_address]() {
            server_delegator(client_fd, client_address);
        });
    };

    std::cout << pool.num_threads << " server delegators are working." << std::endl << std::endl;;
    
    while (g_running) {

        auto [client_fd, client_address] = accept_connection();
        if (client_fd == -2) {
            continue;
        }

        delegator(client_fd, client_address);
    }

    health_worker.join();    // stop the health check worker
    printer_worker.join();   // stop the print info worker
    close(get_server_fd()); // close load balancer socket

    std::cout << std::endl << "Load Balancer on port " << get_port() << " has closed its connection." << std::endl;

    return true;
}