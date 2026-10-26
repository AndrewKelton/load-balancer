#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

#define SMALL_BUFFER_SIZE 128
#define LARGE_BUFFER_SIZE 1024

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

/* Parses a request from the buffer and returns a request_t structure */
request_t parse_request(const std::string& buffer);

/* Parses a response from the buffer and returns a response_t structure */
response_t parse_response(const std::string& buffer);

#endif