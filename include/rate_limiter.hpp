#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <chrono>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <memory>

/* Rate Limiter bucket, allows users application access if
 * they have a token available in their bucket. Tokens refill
 * at a rate of about 2 per second, allowing users essentially
 * 2 requests per second or 120 requests per minute. Can be 
 * fine tuned to other bucket sizes/refill times.
 */
class TokenBucket {

    private:
        std::atomic<int> tokens;
        std::atomic<std::chrono::system_clock::time_point> last_refill; // time the bucket was last refilled

    public:

        TokenBucket(int capacity) {
            tokens.store(capacity);
            last_refill.store(std::chrono::system_clock::now());
        }

        bool allow_request(int rate, int capacity);

        bool try_consume_token();

        /* Returns number of tokens currently in the bucket */
        int get_tokens();   
};

class RateLimiter {

    private:
        
        std::unordered_map<std::string, std::shared_ptr<TokenBucket>> ip_store;
        std::shared_mutex store_mtx;
        int rate_;
        int capacity_;

    public:

        RateLimiter() : rate_(2), capacity_(100) {}

        RateLimiter(double rate, int capacity) : rate_(rate), capacity_(capacity) {}

        bool allow_request(const std::string& ip_address);

        std::shared_ptr<TokenBucket> get_bucket_from_ip(const std::string& ip_address);
};

#endif