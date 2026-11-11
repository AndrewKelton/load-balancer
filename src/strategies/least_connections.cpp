#include <climits>

#include "strategies/least_connections.hpp"

[[nodiscard]] std::optional<size_t> LeastConnections::get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) {

    int least_connections = INT_MAX;
    std::optional<size_t> best_index;

    size_t offset = offset_.load(std::memory_order_relaxed) + 1;

    for (size_t i = offset; i < offset + servers.size(); ++i) {
        size_t curr_index = i % servers.size();

        if (tried.contains(curr_index)) {
            continue; // skip servers that have already tried connecting to
        }

        if (servers[curr_index].status.load(std::memory_order_relaxed) != ServerHealthStatus::OK) {
            continue;
        }

        int current_load = servers[curr_index].active_connections.load(std::memory_order_relaxed);

        if (current_load < least_connections) {
            least_connections = current_load;
            best_index = curr_index;
        }
    }

    offset_.store(offset);

    return best_index;
}