#include "strategies/round_robin.hpp"

[[nodiscard]] std::optional<size_t> RoundRobin::get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) {
            
    for (size_t i = 0; i < static_cast<size_t>(servers.size()); ++i) {
        size_t curr_idx = i % servers.size();

        if (tried.contains(curr_idx)) {
            continue;
        }

        if (servers[curr_idx].status.load() == ServerHealthStatus::OK) {
            return curr_idx;
        }
    }

    return std::nullopt;
}