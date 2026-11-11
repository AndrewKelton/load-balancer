#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <atomic>
#include <netinet/in.h>

#define SMALL_BUFFER_SIZE 128
#define LARGE_BUFFER_SIZE 1024
#define BACKLOG_SIZE 128

struct request_t {
    std::string method;
    std::string path_uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

struct response_t {
    std::string version;
    int status_code;
    std::unordered_map<std::string, std::string> headers;
};

enum class ServerHealthStatus {
    OK,
    DEGRADED,
    FAILED
};

struct server_info_t {
    int port;
    int failure_count = 0;

    /* Initially set to DEGRADED so no requests can "sneak in" right as
     * the load balancer is initially spun up, and checking for x amount
     * of healthy servers that do not exist.
     */
    std::atomic<ServerHealthStatus> status = ServerHealthStatus::DEGRADED; 
    static_assert(std::atomic<ServerHealthStatus>::is_always_lock_free, 
        "ServerHealthStatus atomic must be lock free");
    std::atomic<int> active_connections = 0;
};

/* Parses a request from the buffer and returns a request_t structure */
request_t parse_request(const std::string& buffer);

/* Parses a response from the buffer and returns a response_t structure */
response_t parse_response(const std::string& buffer);

/* Returns lowercase version of input string */
void to_lower(std::string& upper);

/* Returns the string of a binary ip address */
std::string convert_binary_ip_address_to_string(const sockaddr_in& address);

#endif