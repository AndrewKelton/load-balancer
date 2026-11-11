#ifndef ROUND_ROBIN_HPP
#define ROUND_ROBIN_HPP

#include "selection_strategy.hpp"

/* Round robin scheduling is currently bugged. It does not use the
 * previous index to go round-robin. Need to fix this
 */

class RoundRobin : public SelectionStrategy {

    public: 

        /* Returns the next server index from list of servers using the 
         * Round Robin scheduling method. If no server is valid, then
         * std::nullopt will be returned.
         */
        [[nodiscard]] std::optional<size_t> get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) override;
};

#endif