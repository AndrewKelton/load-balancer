#ifndef THREAD_SAFE_QUEUE_HPP
#define THREAD_SAFE_QUEUE_HPP

#include <condition_variable>
#include <mutex>
#include <queue>

// Thread-safe queue
template <typename T>
class ThreadSafeQueue {

    private:

        std::queue<T> queue;
        std::mutex q_mtx;
        std::condition_variable q_cv;
    
    public:

        void push(T item) {
            std::unique_lock<std::mutex> lock(q_mtx);
            queue.push(item);
            q_cv.notify_one();
        }

        T pop() {
            std::unique_lock<std::mutex> lock(q_mtx);
            
            // wait for queue to not be empty
            q_cv.wait(lock, [this]() { return !queue.empty(); });

            T item = queue.front();
            queue.pop();
            
            return item;
        }

        bool empty() {
            std::unique_lock<std::mutex> lock(q_mtx);

            return queue.empty();
        }
};

#endif