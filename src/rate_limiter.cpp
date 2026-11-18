#include "rate_limiter.hpp"

#include <mutex>
#include <iostream>

bool TokenBucket::allow_request(int rate, int capacity) {
    auto now = std::chrono::system_clock::now();

    auto prev = last_refill.load();
    /* calculate how much time has passed since last refill and 
     * refill the bucket with the minimum of capacity and that many
     * tokens + current tokens
     */
    auto elapsed = now - prev;
    std::chrono::duration<double> elapsed_seconds = elapsed;
    int elapsed_rate_refill = std::chrono::duration_cast<std::chrono::seconds>(elapsed_seconds * rate).count();

    if (elapsed_rate_refill >= 1) {
        // only one thread wins claim and adds the tokens
        if (last_refill.compare_exchange_strong(prev, now)) {

            int current_tokens = tokens.load(std::memory_order_relaxed);

            /* keep trying until the swap succeeds; on failure, current_tokens
             * is updated to the latest value and we recompute
             */
            while (!tokens.compare_exchange_weak(current_tokens, 
                                                 std::min(capacity, current_tokens + elapsed_rate_refill),
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
                // empty body: compare_exchange_weak already reloads current_tokens
            }
        }
    }

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

std::shared_ptr<TokenBucket> RateLimiter::get_bucket_from_ip(const std::string& ip_address) {

    std::shared_ptr<TokenBucket> bucket = nullptr;
    
    { // read bucket from ip store
        std::scoped_lock<std::shared_mutex> read_lock(store_mtx);
        auto it = ip_store_.find(ip_address);
        
        if (it != ip_store_.end()) {
            // move accessed ip to the front (mru position)
            bucket = it->second->second;
            ip_list_.erase(it->second);
            ip_list_.push_front({ip_address, bucket});

            // update iterator in map
            ip_store_[ip_address] = ip_list_.begin();
        }
    }

    return bucket;
}   

bool RateLimiter::allow_request(const std::string& ip_address) {
    
    std::shared_ptr<TokenBucket> bucket = nullptr;
    
    {
        std::scoped_lock<std::shared_mutex> lock(store_mtx);

        // read bucket from ip store
        auto it = ip_store_.find(ip_address);
        if (it != ip_store_.end()) {
            bucket = it->second->second;
            // move to front, the iterator stays valid
            ip_list_.splice(ip_list_.begin(), ip_list_, it->second);
        } else {
            // create new bucket and put it on the store
            bucket = std::make_shared<TokenBucket>(capacity_);
            put(ip_address, bucket);
        }
    }

    return bucket->allow_request(rate_, capacity_);
}

void RateLimiter::put(std::string ip, std::shared_ptr<TokenBucket> bucket) {

    auto it = ip_store_.find(ip);
    if (it != ip_store_.end()) {

        // remove old node from the list and map
        ip_list_.erase(it->second);
        ip_store_.erase(it);
    }

    // insert new node at the front of the list
    ip_list_.push_front({ip, bucket});
    ip_store_[ip] = ip_list_.begin();

    // if cache size exceeds the capacity, remove the least recently used ip
    if (ip_store_.size() > ip_store_capacity_) {
        auto last_ip = ip_list_.back().first;
        ip_list_.pop_back();
        ip_store_.erase(last_ip);

        // std::cout << "Evicted" << last_ip << " from ip store.\n";
    }
}