#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <string>

class Server {

    public:

        Server() : port_(-1), server_fd_(-1), address_(-1) {};

        Server(int port) : port_(port) {};

        int setup_socket();

        bool setup_server();

        bool start_listen();

        /* Accepts connection from the client, returns a pair populated
         * with {client file descriptor, client address info}
         */
        std::pair<int, sockaddr_in> accept_connection();

        int get_port();

        int get_server_fd();

        void set_port(int port);

        sockaddr_in get_address();

        ssize_t read_request(int client_fd, std::string& full_client_buffer);

    private:

        int port_;
        int server_fd_;
        sockaddr_in address_;
};

sockaddr_in create_address(int port);

#endif