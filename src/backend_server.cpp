#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <csignal>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>

#include "utils.hpp"
#include "server.hpp"

static volatile std::sig_atomic_t g_running = 1;
static void on_sigint(int) { g_running = 0; }

std::string get_response(request_t request, std::string port) {
    
    // what is our response for this endpoint
    if (request.path_uri == "/health") {
        return "HTTP/1.1 200 OK\r\n"; // as of now just send healthy response
    } 

    /* I need to make a stand alone function to do this later on... */
    // split the request by its paramters and extract them
    std::vector<std::string_view> parameters = split(request.path_uri, "?");

    // std::vector<std::string_view> parameters = split(split(request.path_uri, "?")[1], "&");

    // endpoint to sleep this server for x ms, max is 2900ms
    if (request.path_uri.contains("/slow")) {
        int sleep_time = 2900; // max is 2900 because the server times out after 3s
        int length = 4;
        if (parameters.size() > 1) {
            std::vector<std::string_view> param_to_value = split(parameters[1], "=");

            sleep_time = std::min(2900, to_int(param_to_value[1]).value());
            length = static_cast<int>(param_to_value[1].size());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

        return 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plai\r\n"
            "Content-Length: " + std::to_string(12 + length) + "\r\n\r\n"
            "Slept for " + std::to_string(sleep_time) + "ms";
    }

    std::string body = "Hello from port: " + port;
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
}

int main(int argc, char * argv[]) {

    if (argc < 2) {
        std::cerr << "No port specified" << std::endl;
        return 1;
    }

    // signal to stop server loop
    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART, so accept() returns EINTR
    sigaction(SIGINT, &sa, nullptr);

    int port = std::stoi(argv[1]);
    std::string port_string = argv[1];

    Server server(port);
    server.setup_server();
    server.start_listen();

    // 4. Accept a connection loop
    while (g_running) {
        
        auto [client_fd, client_address] = server.accept_connection();
        if (client_fd < 0) {
            continue;
        }

        // Read incoming request data
        std::string full_buffer;
        // char buffer[LARGE_BUFFER_SIZE] = {0};
        if (server.read_request(client_fd, full_buffer) <= 0 ) {
            close(client_fd);
            continue;
        }
        /* Have to do this differently for read(), need to read until blank
         * line "\r\n\r\n" is received. 
         */

        std::cout << "Received Request: " << std::endl << full_buffer << std::endl;

        std::string response; 
        request_t request = parse_request(full_buffer); // parse the request
        if (request.method.empty()) {
            response = "HTTP/1.1 400 Bad Request"; // request was malformed
        } else {
            response = get_response(request, port_string); // get the response
        }

        write(client_fd, response.c_str(), response.size());
        close(client_fd); // close connection for this request
    }

    close(server.get_server_fd()); // close server socket

    std::cout << std::endl << "Server on port " << port << " has closed its connection." << std::endl;

    return 0;
}