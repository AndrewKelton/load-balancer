#ifndef ROUND_ROBIN_HPP
#define ROUND_ROBIN_HPP

#include "selection_strategy.hpp"

#include <atomic>

/* Round robin scheduling is currently bugged. It does not use the
 * previous index to go round-robin. Need to fix this
 */

class RoundRobin : public SelectionStrategy {

    private:

        /* Similar offset to the LeastConnections class where we start
         * from the next index that was previously chosen, except we
         * just pick the healthy server that comes "next".
         */
        std::atomic<size_t> next_ = 0;

    public: 

        /* Returns the next server index from list of servers using the 
         * Round Robin scheduling method. If no server is valid, then
         * std::nullopt will be returned.
         */
        [[nodiscard]] std::optional<size_t> get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) override;
};

#endif