#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <chrono>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <memory>

/* Next steps are to add the rate limiter to the Load Balancer.
 * Will we start with the rate limiter attached to the load balancer
 * as a simple proxy for the traffic. Eventually, putting the rate
 * limiter at the application level may be useful when dealing with 
 * different types of requests, but since right now we only handle 
 * /health requests this will do and be a good way to check the 
 * functionality of the rate limiter.
 */


/* Rate Limiter bucket, allows users application access if
 * they have a token available in their bucket. Tokens refill
 * at a rate of about 2 per second, allowing users essentially
 * 2 requests per second or 120 requests per minute. Can be 
 * fine tuned to other bucket sizes/refill times.
 */
class TokenBucket {

    private:
        std::atomic<int> tokens;
        std::__1::chrono::system_clock::time_point last_refill; // time the bucket was last refilled

    public:

        TokenBucket(int capacity) {
            tokens.store(capacity);
            last_refill = std::chrono::system_clock::now();
        }

        bool allow_request(double rate, int capacity);

        bool try_consume_token();
};

class RateLimiter {

    private:
        
        std::unordered_map<std::string, std::shared_ptr<TokenBucket>> ip_store;
        std::shared_mutex store_mtx;
        double rate_;
        int capacity_;

        // do rate limiter stuff here

    public:

        RateLimiter() : rate_(2.0), capacity_(100) {}

        RateLimiter(double rate, int capacity) : rate_(rate), capacity_(capacity) {}

        bool allow_request(const std::string& ip_address);
};

#endif