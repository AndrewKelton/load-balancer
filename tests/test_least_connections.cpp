#include <gtest/gtest.h>

#include "strategies/least_connections.hpp"
#include "utils.hpp"

#include <thread>
#include <vector>
#include <set>
#include <optional>

#define TEST_NUM_SERVERS 3

TEST(LeastConnectionsDistribution, LeastConnectionsDistributesDefaultCorrectlyServersHealthy) {

    LeastConnections least_connections;

    std::vector<server_info_t> servers;
    servers = std::vector<server_info_t>(TEST_NUM_SERVERS);
    for(int i = 0; i < TEST_NUM_SERVERS; ++i) {
        servers[i].port = 9000 + i;
        servers[i].status.store(ServerHealthStatus::OK);
    }

    std::set<size_t> tried;
    for (int i = 0; i < 6; ++i) {
        // least connections starts at offset 1
        EXPECT_EQ(least_connections.get_server_index(servers, tried), (i + 1) % TEST_NUM_SERVERS);
    }
}

TEST(LeastConnectionsDistribution, LeastConnectionsDistributesCorrectlyUnderLoadServersHealthy) {

    LeastConnections least_connections;

    std::vector<server_info_t> servers;
    servers = std::vector<server_info_t>(TEST_NUM_SERVERS);
    for(int i = 0; i < TEST_NUM_SERVERS; ++i) {
        servers[i].port = 9000 + i;
        servers[i].status.store(ServerHealthStatus::OK);
    }

    // set active connections
    servers[0].active_connections.store(10);
    servers[1].active_connections.store(15);
    servers[2].active_connections.store(20);

    std::set<size_t> tried;
    for (int i = 0; i < TEST_NUM_SERVERS; ++i) {
        EXPECT_EQ(least_connections.get_server_index(servers, tried), i);
        servers[i].active_connections.store(100);
    }
}

TEST(LeastConnectionsDistribution, LeastConnectionsDistributesNoServerHealthy) {

    LeastConnections least_connections;

    std::vector<server_info_t> servers;
    servers = std::vector<server_info_t>(TEST_NUM_SERVERS);
    for(int i = 0; i < TEST_NUM_SERVERS; ++i) {
        servers[i].port = 9000 + i;
    }

    std::set<size_t> tried;
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(least_connections.get_server_index(servers, tried), std::nullopt);
    }
}