#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <string>
#include <optional>

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

        bool server_is_listening();
        
        sockaddr_in get_address();
        
        void set_port(int port);

        ssize_t read_request(int client_fd, std::string& full_client_buffer);

    private:

        int port_;
        int server_fd_;
        sockaddr_in address_;
        bool is_listening = false;
};

/* Creates and returns a sockaddr_in address */
std::optional<sockaddr_in> create_address(int port);

/* Sends all bytes to fd until all bytes are sent. 
 * Returns true if it was successful, and false if some
 * error such as ::send() returning < 0 occurs.
 */
bool send_all(int fd, const char * data, size_t len);

#endif