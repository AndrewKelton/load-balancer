#include <gtest/gtest.h>

#include "utils.hpp"

#include <vector>
#include <string_view>

TEST(Utils, ContentLengthIsFound) {
    
    std::vector<std::string_view> content_split = split("Content-Length: 23", ":");
    size_t content_length = to_size_t(trim(content_split[1])).value();

    EXPECT_EQ(content_length, 23);
}