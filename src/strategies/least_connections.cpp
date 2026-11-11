#include <climits>

#include "strategies/least_connections.hpp"

[[nodiscard]] std::optional<size_t> LeastConnections::get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) {

    int least_connections = INT_MAX;
    std::optional<size_t> best_index;

    for (size_t i = 0; i < servers.size(); ++i) {

        if (tried.contains(i)) {
            continue; // skip servers that have already tried connecting to
        }

        if (servers[i].status.load(std::memory_order_relaxed) != ServerHealthStatus::OK) {
            continue;
        }

        int current_load = servers[i].active_connections.load(std::memory_order_relaxed);

        if (current_load < least_connections) {
            least_connections = current_load;
            best_index = i;
        }
    }

    return best_index;
}