#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <chrono>
#include <atomic>

/* Rate Limiter bucket, allows users application access if
 * they have a token available in their bucket. Tokens refill
 * at a rate of about 2 per second, allowing users essentially
 * 2 requests per second or 120 requests per minute. Can be 
 * fine tuned to other bucket sizes/refill times.
 */
class TokenBucket {

    private:
        double rate;
        int capacity;
        std::atomic<int> tokens;
        std::__1::chrono::system_clock::time_point last_refill; // time the bucket was last refilled


    public:

        TokenBucket(double rate, int capacity) {
            this->rate = rate;
            this->capacity = capacity;
            tokens.store(this->capacity);
            last_refill = std::chrono::system_clock::now();
        }

        bool allow_request();

        bool try_consume_token();
};

class RateLimiter {

    private:
        
        TokenBucket bucket;

        // do rate limiter stuff here

    public:

        // do rate limiter stuff here
};

#endif