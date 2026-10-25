
#include "utils.hpp"

std::unordered_map<std::string, std::string> parse_request_headers(std::vector<std::string> lines) {
    std::unordered_map<std::string, std::string> headers;

    for (auto line : lines) {
        int type_end = line.find(':');
        headers[line.substr(0, type_end)] = line.substr(type_end + 2);
    }

    return headers;
}

request_t parse_request_line(const std::string_view& line, std::vector<std::string> lines) {
    
    int method_end = line.find(' ');
    int path_end = line.substr(method_end + 1).find(' ');

    request_t request;
    request.method = line.substr(0, method_end);
    request.path_uri = line.substr(method_end + 1, path_end);
    request.version = line.substr(path_end + 1);
    request.headers = parse_request_headers(lines);

    return request;
}

request_t parse_request(const std::string& buffer) {
    
    std::vector<size_t> indices;
    std::vector<std::string> lines;

    indices.push_back(0);
    int prev = 0;

    size_t position = buffer.find('\n');

   while (position != std::string::npos) {
        // std::cout << buffer.substr(indices[prev++], position - 1);
        lines.push_back(buffer.substr(indices[prev++], position - 1));
        indices.push_back(position);

        position = buffer.find('\n', position + 1);
        // position = buffer.find("\r\n", position + 2);
    }

    request_t request = parse_request_line(lines[0], std::vector<std::string>(lines.begin() + 1, lines.end()));

    return request;
}