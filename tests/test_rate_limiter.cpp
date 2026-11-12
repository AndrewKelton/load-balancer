#include <gtest/gtest.h>

#include "rate_limiter.hpp"

#include <chrono>
#include <thread>
#include <atomic>

TEST(RateLimit, RateLimiterAllowsRequests) {

    RateLimiter rate_limiter;

    EXPECT_EQ(rate_limiter.allow_request("10.0.0.1"), true);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.2"), true);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.3"), true);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.1"), true);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.2"), true);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.3"), true);
}   

TEST(RateLimit, RateLimiterDeniesRequests) {

    RateLimiter rate_limiter(0, 1);
    
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.1"), true);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.1"), false);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.2"), true);
}

TEST(RateLimit, RateLimiterConcurrentIPConsumption) {
    
    RateLimiter rate_limiter(0, 3);
    std::thread threads[3];

    std::atomic<int> allowed_requests = 0;
    for (int i = 0; i < 3; ++i) {
        threads[i] = std::thread([&rate_limiter, &allowed_requests]() {
            if (rate_limiter.allow_request("10.0.0.1")) {
                allowed_requests.fetch_add(1);
            }
        });
    }

    // ensure that bucket was not filled
    if (rate_limiter.allow_request("10.0.0.1")) {
        allowed_requests.fetch_add(1);
    }

    for (int i = 0; i < 3; ++i) {
        threads[i].join();
    }

    EXPECT_EQ(allowed_requests, 3);
}

TEST(RateLimitBucket, RateLimitBucketRefills) {

    RateLimiter rate_limiter;

    while (rate_limiter.allow_request("10.0.0.1")) {}

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.1"), true);
}

TEST(RateLimitBucket, RateLimitBucketRefillCapacity) {

    RateLimiter rate_limiter(2, 2);

    while (rate_limiter.allow_request("10.0.0.1")) {}

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // lazily fills, so should be empty, even though enough time has passed
    EXPECT_EQ(rate_limiter.get_bucket_from_ip("10.0.0.1")->get_tokens(), 0);
    EXPECT_EQ(rate_limiter.allow_request("10.0.0.1"), true);
    EXPECT_EQ(rate_limiter.get_bucket_from_ip("10.0.0.1")->get_tokens(), 1);
}
