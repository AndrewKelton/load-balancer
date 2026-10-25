#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

struct request_t {
    std::string method;
    std::string path_uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

request_t parse_request(const std::string& buffer);

#endif