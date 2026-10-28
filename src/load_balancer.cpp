#include "load_balancer.hpp"
#include "utils.hpp"
#include "thread_safe_queue.hpp"

#include <cstring>

/* Next steps:
 * (1) Have workers that delegate the requests from the client so that we can accept multiple clients 
 * at the same time while ensuring thread and request safety.
 *  - create a thread safe queue
 *  - create thread safe backend selection
 *  - bounded queue? what happens when queue is full?
 *  - increase backlog
 *  - ensure that queue access and backend selection are thread-safe without locking the entire request lifecycle.
 * (3) Make a round robin function, kinda like get_next_healthy_server(), but return the server to use.
 * (4) Make a least connections function, different method of delegating work, this should test actual
 * concurrent connections and use atomics.
 *  - This requires keeping track of how many clients are connected to that server at once.
 *  - Need to make it non-blocking
 */

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
        for (int i = 0; i < MAX_NUM_SERVERS; ++i) {
            send_health_check(this->servers.at(i));
        }

        std::this_thread::sleep_for(std::chrono::seconds(HEALTH_CHECK_INTERVAL_S));
    }
}

int8_t load_balancer::LoadBalancer::get_next_server(int8_t server_idx) {

    for (int8_t i = 0; i < MAX_NUM_SERVERS; ++i) {
        int8_t curr_idx = (server_idx + i) % MAX_NUM_SERVERS;

        if (this->servers[curr_idx].status.load() == ServerHealthStatus::OK) {
            return curr_idx;
        }
    }

    return -1;
}

void load_balancer::LoadBalancer::server_delegator(int client_fd, int8_t curent_server_idx) {

    std::string full_client_buffer;
    char client_buffer[LARGE_BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, client_buffer, LARGE_BUFFER_SIZE);

    if (bytes_read <= 0) { // nothing read
        close(client_fd);
        return;
    }

    full_client_buffer.append(client_buffer, bytes_read);

    int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    std::string response;

    int chosen_server_idx = get_next_server(curent_server_idx);

    if (chosen_server_idx < 0) {
        // no servers are active
        std::cerr << "No servers available." << std::endl;
        response = "HTTP/1.1 503 Service Unavailable\r\n";
    } else {
        // some server is active
        sockaddr_in server_address = create_address(this->servers[chosen_server_idx].port);
        
        int connection_status = connect(backend_fd, (struct sockaddr*)&server_address, sizeof(server_address));
        
        // fallback, try to find a server that will connect with round robin
        int retries = 1;
        while (retries < MAX_NUM_SERVERS && connection_status < 0) {
            close(backend_fd);
            ++retries;

            chosen_server_idx = get_next_server(chosen_server_idx + 1);
            if (chosen_server_idx < 0) {
                break; // no servers are active now
            }

            backend_fd = socket(AF_INET, SOCK_STREAM, 0);
            server_address = create_address(this->servers[chosen_server_idx].port);
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
            std::cout << "Connected to server on port: " << this->servers[chosen_server_idx].port << std::endl;

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

            close(backend_fd); // close backend socket
        }
    }
    write(client_fd, response.c_str(), response.size());
    close(client_fd); // close client socket
}

ThreadSafeQueue<int> queue; // thread-safe queue to hold client file descriptors

void load_balancer::LoadBalancer::server_delegator_worker(volatile std::sig_atomic_t& g_running, int thread_id) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(this->delegator_mtx);
            this->delegator_cv.wait(lock, [this, &g_running]() { return !queue.empty() || !g_running; });
        }

        if (!g_running) {
            break;
        }

        std::cout << "Thread id: " << thread_id << std::endl;
        server_delegator(queue.pop(), this->current_server.load());
    }
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

    // create worker threads
    static unsigned int num_threads = std::thread::hardware_concurrency() * 2;
    std::vector<std::thread> server_delegator_workers;
    server_delegator_workers.reserve(num_threads);

    for (int8_t i = 0; i < num_threads; ++i) {
        server_delegator_workers.emplace_back([this, &g_running, i]() {
            server_delegator_worker(g_running, i); 
        });
    }

    std::cout << num_threads << " server delegators are working." << std::endl << std::endl;;
    
    while (g_running) {

        int address_len = sizeof(this->address); // get address length

        int client_fd = accept(this->load_balancer_fd,  (struct sockaddr*)&this->address, (socklen_t*)&address_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            continue;
        }
        queue.push(client_fd);

        std::cout << "Received request from client" << std::endl;

        /* Instead of calling this function directly, need to make a
         * thread pool where workers are waiting to be woken up to work
         * with a client file descriptor.
         */
        {
            std::unique_lock<std::mutex> lock(this->delegator_mtx);
            this->delegator_cv.notify_one();
        }
        
        // move to next index of servers
        if (this->current_server.load() < MAX_NUM_SERVERS - 1) {
            this->current_server.fetch_add(1);
        } else {
            this->current_server.store(0);
        }
    }

    health_worker.join(); // stop the health check worker

    {
        std::unique_lock<std::mutex> lock(this->delegator_mtx);
        this->delegator_cv.notify_all(); // wake up all delegator threads
    }
    for (int8_t i = 0; i < num_threads; ++i) { // join all delegator threads
        server_delegator_workers[i].join();
    }

    close(this->load_balancer_fd); // close load balancer socket

    std::cout << std::endl << "Load Balancer on port " << this->port << " has closed its connection." << std::endl;

    return true;
}