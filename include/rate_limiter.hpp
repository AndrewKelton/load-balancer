#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <chrono>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <list>

#define DEFAULT_IP_STORE_CAPACITY 2 // set low for now to view how it works

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

/* RateLimiter class an LRU eviction policy, and I plan on adding 
 * a TTL policy on top of it. Though the ip store size is defaulted
 * at 5, I plan on changing the default to 10,000 to be more 
 * production-esque for a smaller server.
 * 
 * The rate limiter stores each ip and its token bucket as an Entry
 * in doubly linked list to preserve the LRU eviction policy. When a
 * ip is accessed it is moved to the front of the list. Though someone
 * DDoSing the server will just take over all the MRU spaces in the 
 * list, so I need to figure out a way to have ips be classified into 
 * 3 different categories: Good, Suspect, Bad. Allowing Good ips access,
 * requiring Suspect ips more authentification, and denying Bad ips.
 */
class RateLimiter {

    using Entry = std::pair<std::string, std::shared_ptr<TokenBucket>>;

    private:
        
        std::list<Entry> ip_list_; // preservers ordering of ip usage
        // map from key to list iterator
        std::unordered_map<std::string, std::list<Entry>::iterator> ip_store_;

        std::shared_mutex store_mtx;
        int rate_;
        int capacity_;
        size_t ip_store_capacity_;

        /* Puts a ip address and its bucket into the list*/
        void put(std::string ip, std::shared_ptr<TokenBucket> bucket);

    public:

        RateLimiter() : rate_(2), capacity_(100), ip_store_capacity_(DEFAULT_IP_STORE_CAPACITY) {}

        RateLimiter(int rate, int capacity) : 
            rate_(rate), 
            capacity_(capacity), 
            ip_store_capacity_(DEFAULT_IP_STORE_CAPACITY) {}

        RateLimiter(int rate, int capacity, size_t ip_store_capacity) : 
            rate_(rate), 
            capacity_(capacity), 
            ip_store_capacity_(ip_store_capacity) {}

        bool allow_request(const std::string& ip_address);

        /* Returns TokenBucket associated with an ip address or nullptr if that
         * ip address is not in the ip store.
         */
        std::shared_ptr<TokenBucket> get_bucket_from_ip(const std::string& ip_address);
};

#endif