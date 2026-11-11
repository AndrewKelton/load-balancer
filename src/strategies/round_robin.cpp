#include "strategies/round_robin.hpp"

[[nodiscard]] std::optional<size_t> RoundRobin::get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) {
            
    size_t next = next_.load(std::memory_order_relaxed);
    for (size_t i = next; i < next + static_cast<size_t>(servers.size()); ++i) {
        size_t curr_idx = i % servers.size();

        if (tried.contains(curr_idx)) {
            continue;
        }

        if (servers[curr_idx].status.load() == ServerHealthStatus::OK) {
            next_.store(curr_idx + 1);
            return curr_idx;
        }
    }

    return std::nullopt;
}