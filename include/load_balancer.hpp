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

#include "utils.hpp"

#define MAX_NUM_SERVERS 3
#define MAX_FAILURE_COUNT 5
#define HEALTH_CHECK_INTERVAL_S 2

namespace load_balancer {

    enum class ServerHealthStatus {
        OK,
        DEGRADED,
        FAILED
    };

    /* Returns a ServerHealthStatus as a std::string 
     * OK => "OK", DEGRADED => "DEGRADED", FAILED => "FAILED"
     */
    std::string server_health_status_to_string(load_balancer::ServerHealthStatus status);

    // health endpoint
    inline constexpr const char * health_endpoint = "GET /health HTTP/1.1\r\n";

    struct server_info_t {
        int port;
        int failure_count = 0;
        std::atomic<ServerHealthStatus> status = ServerHealthStatus::OK;
        static_assert(std::atomic<ServerHealthStatus>::is_always_lock_free, 
            "ServerHealthStatus atomic must be lock free");
        std::atomic<int> active_connections = 0;
    };


    class LoadBalancer {
        
        private:

            std::vector<server_info_t> servers; // this might be better as a circular linked list
            int load_balancer_fd; // server_fd socket for load balancer
            sockaddr_in address; // address for load balancer
            std::atomic<int> current_server; // current server index for active server
            int port; // port to connect to ex. 8080
            std::string load_method = "least-connections";

            /* Creates address for a socket */
            sockaddr_in create_address(int port);

            /* Connects to the server specified and sends a health check using 
            * the /health endpoint. If a server returns 200 OK, then that server
            * is considered HEALTHY, if it returns anything else, then it is DED.
            */
            void send_health_check(server_info_t& server);

            /* worker to send health checks to the servers every 2 seconds */
            void health_checker(volatile std::sig_atomic_t& g_running);

            /* Locates and sets current_server to the index of the next healthy server 
            * in the list of servers. If no healthy server is found in the list of 
            * servers, returns -1.
            */
            int get_next_server(int server_idx);

            /* Delegates the client's request to the appropriate server using
             * the Round Robin scheduling method. 
             */
            void round_robin_server_delegator(int client_fd, int curent_server_idx);

            /* Delegates the client's request to the server with the least active
             * connections. Using the Least Connections scheduling method.
             */
            void least_connections_server_delegator(int client_fd);

            /* Returns index from servers vector with the least number of active connections */
            int get_server_index_with_least_connections(std::set<int> server_indexes_tried);

            /* Sends request to backend and receives response and saves it to 'response'. */
            void send_request_recv_response(int backend_fd, std::string client_buffer, std::string& response);
            
            /* Prints the load balancer's info, like servers connected to, status, connections, etc.*/
            void print_load_balancer_info();

            /* Print worker that prints the load balancer's info every second */
            void print_load_balancer_info_worker(volatile std::sig_atomic_t& g_running);

        public:

            LoadBalancer(std::vector<int> backend_ports, int port = 8080) {
                
                current_server = 0;
                this->port = port;
                servers = std::vector<server_info_t>(MAX_NUM_SERVERS);

                for (int i = 0; i < MAX_NUM_SERVERS; ++i) {
                    servers[i].port = backend_ports[i];
                }

                if (!setup_server(port)) {
                    std::cerr << "Error setting up server!" << std::endl;
                    exit(1);
                }
            }

            LoadBalancer(std::vector<int> backend_ports, int port, std::string load_method) {
                
                current_server = 0;
                this->port = port;
                servers = std::vector<server_info_t>(MAX_NUM_SERVERS);
                
                this->load_method = load_method;
                to_lower(this->load_method);

                for (int i = 0; i < MAX_NUM_SERVERS; ++i) {
                    servers[i].port = backend_ports[i];
                }

                if (!setup_server(port)) {
                    std::cerr << "Error setting up server!" << std::endl;
                    exit(1);
                }
            }

            /* Creates server socket and server specifications. */
            bool setup_server(int port = 8080);

            /* Starts listening for incoming connections and loops waiting for incoming
            * client connections. */
            bool start_listen(volatile std::sig_atomic_t& g_running);
    };
};

#endif