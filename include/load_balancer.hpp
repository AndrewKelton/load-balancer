#ifndef LOAD_BALANCER_HPP
#define LOAD_BALANCER_HPP

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <set>
#include <memory>

#include "utils.hpp"
#include "server.hpp"
#include "strategies/round_robin.hpp"
#include "strategies/least_connections.hpp"

#define MAX_NUM_SERVERS 3
#define MAX_FAILURE_COUNT 5
#define HEALTH_CHECK_INTERVAL_S 2

namespace load_balancer {

    /* Returns a ServerHealthStatus as a std::string 
     * OK => "OK", DEGRADED => "DEGRADED", FAILED => "FAILED"
     */
    std::string server_health_status_to_string(ServerHealthStatus status);

    // health endpoint
    inline constexpr const char * health_endpoint = "GET /health HTTP/1.1\r\n\r\n";

    class LoadBalancer : public Server {
        
        private:

            std::vector<server_info_t> servers; // this might be better as a circular linked list
            std::atomic<int> current_server; // current server index for active server
            std::string load_method = "least-connections";

            std::unique_ptr<SelectionStrategy> strategy;

            /* Connects to the server specified and sends a health check using 
            * the /health endpoint. If a server returns 200 OK, then that server
            * is considered HEALTHY, if it returns anything else, then it is DED.
            */
            void send_health_check(server_info_t& server);

            /* worker to send health checks to the servers every 2 seconds */
            void health_checker(volatile std::sig_atomic_t& g_running);

            /* Delegates the client's request to the server using the scheduling strategy
             * pointed to by LoadBalancer::strategy
             */
            void server_delegator(int client_fd);

            /* Sends request to backend and receives response and saves it to 'response'. */
            void send_request_recv_response(int backend_fd, std::string client_buffer, std::string& response);
            
            /* Prints the load balancer's info, like servers connected to, status, connections, etc.*/
            void print_load_balancer_info();

            /* Print worker that prints the load balancer's info every second */
            void print_load_balancer_info_worker(volatile std::sig_atomic_t& g_running);

        public:

            LoadBalancer(std::vector<int> backend_ports, int port, std::string load_method) {

                current_server = 0;
                set_port(port);
                servers = std::vector<server_info_t>(MAX_NUM_SERVERS);
                
                this->load_method = load_method;
                to_lower(this->load_method);

                if (this->load_method == "round-robin") {
                    strategy = std::make_unique<RoundRobin>();
                } else {
                    strategy = std::make_unique<LeastConnections>(); // least connections by default
                }

                for (int i = 0; i < MAX_NUM_SERVERS; ++i) {
                    servers[i].port = backend_ports[i];
                }

                if (!setup_server()) {
                    std::cerr << "Error setting up server!" << std::endl;
                    exit(1);
                }
            }

            /* Starts listening for incoming connections and loops waiting for incoming
            * client connections. */
            bool start_load_balancer(volatile std::sig_atomic_t& g_running);
    };
};


#endif