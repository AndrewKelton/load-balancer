#ifndef LEAST_CONNECTIONS_HPP
#define LEAST_CONNECTIONS_HPP

#include "selection_strategy.hpp"

class LeastConnections : public SelectionStrategy {

    public:     

        /* Returns the next server index from list of servers using the 
         * Least Connections scheduling method. If no server is valid, then
         * std::nullopt will be returned.
         */
        [[nodiscard]] std::optional<size_t> get_server_index(std::vector<server_info_t>& servers, const std::set<size_t>& tried) override;
};

#endif