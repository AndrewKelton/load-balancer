#ifndef LOAD_BALANCER_HPP
#define LOAD_BALANCER_HPP

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

namespace load_balancer {

enum class ServerHealthStatus {
    OK,
    DEGRADED,
    FAILED
};

inline constexpr const char * health_endpoint = "GET /health HTTP/1.1\r\n";

struct server_info_t {
    int port;
    int failure_count = 0;
    std::atomic<ServerHealthStatus> status = ServerHealthStatus::OK;
    static_assert(std::atomic<ServerHealthStatus>::is_always_lock_free, 
        "ServerHealthStatus atomic must be lock free");
};

class LoadBalancer {
    private:
    
        std::vector<server_info_t> servers; // this might be better as a circular linked list
        int socket_lb;
        sockaddr_in address;
        int current_server;
        int port;

        /* Creates address for a socket */
        sockaddr_in create_address(int port);

        /* Connects to the server specified and sends a health check using 
        * the /health endpoint. If a server returns 200 OK, then that server
        * is considered HEALTHY, if it returns anything else, then it is DED.
        */
        void send_health_check(server_info_t& server);

        /* worker to send health checks to the servers every 2 seconds */
        void health_checker();

        /* Locates and sets current_server to the index of the next healthy server 
         * in the list of servers. If no healthy server is found in the list of 
         * servers, returns -1.
        */
        bool get_next_healthy_server();

    public:

        LoadBalancer(std::vector<int> backend_ports, int port = 8080) {
            this->current_server = 0;
            this->port = port;
            this->servers = std::vector<server_info_t>(backend_ports.size());

            for (int i = 0; i < backend_ports.size(); ++i) {
                this->servers[i].port = backend_ports[i];
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
        bool start_listen();
    };
};

#endif