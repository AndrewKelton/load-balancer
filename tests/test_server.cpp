#include <gtest/gtest.h>

#include "server.hpp"

TEST(CreateAddress, ValidPortReturnAddress) {
    EXPECT_NE(create_address(9001), std::nullopt);
}

TEST(CreateAddress, InvalidPortReturnsNullopt) {
    EXPECT_EQ(create_address(-1), std::nullopt);
}