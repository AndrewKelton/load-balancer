#include "rate_limiter.hpp"

bool TokenBucket::allow_request(double rate, int capacity) {
    auto now = std::chrono::system_clock::now();

    /* calculate how much time has passed since last refill and 
     * refill the bucket with the minimum of capacity and that many
     * tokens + current tokens
     */
    auto elapsed = now - last_refill;
    std::chrono::duration<double> elapsed_seconds = elapsed;
    int elapsed_rate_refill = std::chrono::duration_cast<std::chrono::seconds>(elapsed_seconds * rate).count();
    
    int tmp_tokens = std::min(capacity, tokens + elapsed_rate_refill); // max tokens can be capacity
    tokens.store(tmp_tokens); // store current tokens in bucket

    last_refill = now;

    return try_consume_token();
}

bool TokenBucket::try_consume_token() {
    
    int current = tokens.load(std::memory_order_relaxed);

    // loop until successfully subtract token or there are none
    while (current >= 1) {
        if (tokens.compare_exchange_weak(current, current - 1,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
            return true; // successfully consumed token
        }
    }

    return false; // no tokens available
}

int TokenBucket::get_tokens() {
    return tokens.load(std::memory_order_relaxed);
}

bool RateLimiter::allow_request(const std::string& ip_address) {

    std::shared_ptr<TokenBucket> bucket = nullptr;

    { // read bucket from ip store
        std::shared_lock<std::shared_mutex> read_lock(store_mtx);
        auto it = ip_store.find(ip_address);
        
        if (it != ip_store.end()) {
            bucket = it->second;
        }
    }

    // if ip is not in store, add it to store
    if (!bucket) {
        std::shared_lock<std::shared_mutex> write_lock(store_mtx);

        // double check to make sure no other thread added to store
        auto it = ip_store.find(ip_address);

        if (it == ip_store.end()) {
            bucket = std::make_shared<TokenBucket>(capacity_);
            ip_store[ip_address] = bucket;
        } else {
            bucket = it->second;
        }
    }
    
    return bucket->allow_request(rate_, capacity_); // check if bucket has token
}

std::shared_ptr<TokenBucket> RateLimiter::get_bucket_from_ip(const std::string& ip_address) {

    std::shared_ptr<TokenBucket> bucket = nullptr;
    
    { // read bucket from ip store
        std::shared_lock<std::shared_mutex> read_lock(store_mtx);
        auto it = ip_store.find(ip_address);
        
        if (it != ip_store.end()) {
            bucket = it->second;
        }
    }

    return bucket;
}   