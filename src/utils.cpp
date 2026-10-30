
#include "utils.hpp"

#include <string_view>
#include <algorithm>
#include <cctype>

namespace {
    /* Strip leading + trailing white space and tabs
     */
    std::string_view trim(std::string_view s) {

        const char * ws = " \t";
        size_t start = s.find_first_not_of(ws);

        if (start == std::string_view::npos) {
            return {};
        }

        size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }

    /* Split the buffer into lines on "\n", dropping a trialing "\r" from each.
     * Stops at the first empty line (end of headers) or end of buffer.
     */
    std::vector<std::string_view> split_header_lines(std::string_view buffer) {
        
        std::vector<std::string_view> lines;
        size_t start = 0;

        while (start < buffer.size()) {
            size_t nl = buffer.find('\n', start);
            size_t end = (nl == std::string_view::npos) ? buffer.size() : nl;

            std::string_view line = buffer.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }

            if (line.empty()) {
                break; // end of headers
            }

            lines.push_back(line);

            if (nl == std::string_view::npos) {
                break;
            } 

            start = nl + 1; // next line begins after '\n'
        }

        return lines;
    }

    /* "GET /path HTTP 1.1" -> method, path, version. Returns false if malformed 
     */
    bool parse_request_line(std::string_view line, request_t& request) {

        size_t sp1 = line.find(' ');
        if (sp1 == std::string_view::npos) {
            return false;
        }
        
        size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos) {
            return false;
        }

        request.method = std::string(line.substr(0, sp1));
        request.path_uri = std::string(line.substr(sp1 + 1, sp2 - sp1 - 1));
        request.version = std::string(line.substr(sp2 + 1));

        return !request.method.empty() && !request.path_uri.empty() && !request.version.empty();
    }
}

request_t parse_request(const std::string& buffer) {
    
    request_t request;
    std::vector<std::string_view> lines = split_header_lines(buffer);

    if (lines.empty() || !parse_request_line(lines[0], request)) {
        return request; // empty request_t signals malformed request
    }

    for (size_t i = 1; i < lines.size(); ++i) {
        size_t colon = lines[i].find(':');
        if (colon == std::string_view::npos) {
            continue; // skip malfomed header line
        }

        std::string_view key = trim(lines[i].substr(0, colon));
        std::string_view value = trim(lines[i].substr(colon + 1));

        if (!key.empty()) {
            request.headers[std::string(key)] = std::string(value);
        }
    }

    return request;
}

void to_lower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
}