#pragma once

#include "types.hpp"
#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"

#include <iostream>
#include <print>

#include <memory>

namespace dispatcher
{
    using namespace queue;
    using namespace thread_pool;

    class TaskDispatcher
    {
    private:
        std::shared_ptr<PriorityQueue> p_queue_;
        ThreadPool pool_;

    public:
        TaskDispatcher(size_t n_threads = 
                std::max(std::thread::hardware_concurrency() - 1, 1u), 
            PriorityQueue::Config&& config = 
                {{TaskPriority::High, QueueOptions{1000}}, 
                {TaskPriority::Normal, QueueOptions{std::nullopt}}});

        void schedule(TaskPriority priority, std::function<void()> task);
        ~TaskDispatcher();
    };
}  // namespace dispatcher