#pragma once

#include "queue/priority_queue.hpp"

#include <thread>

namespace dispatcher::thread_pool
{
    using namespace queue;

    class ThreadPool
    {
    private:
        std::shared_ptr<PriorityQueue> p_queue_;
        std::vector<std::jthread> threads_;

        std::atomic_flag stop_ = ATOMIC_FLAG_INIT;

        void run();
        
    public:
        ThreadPool(std::shared_ptr<PriorityQueue> p_queue, 
            size_t n_threads);
        ~ThreadPool();
    };
} // namespace dispatcher::thread_pool