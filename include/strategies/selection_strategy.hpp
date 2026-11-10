// selection_strategy.hpp
#pragma once

#ifndef SELECTION_STRATEGY_HPP
#define SELECTION_STRATEGY_HPP

#include <vector>
#include "utils.hpp"

class SelectionStrategy {

    public:

        /* Returns the server index for valid server based on whatever
         * scheduling method implements it. If no server is available,
         * then returns std::nullopt
         */
        virtual std::optional<size_t> get_server_index(std::vector<server_info_t>& servers) = 0;

        // destructor
        virtual ~SelectionStrategy() = default;
};

#endif