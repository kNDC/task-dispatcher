#include "thread_pool/thread_pool.hpp"

namespace dispatcher::thread_pool
{
    ThreadPool::ThreadPool(std::shared_ptr<PriorityQueue> p_queue, 
        size_t n_threads) : 
        p_queue_{p_queue}
    {
        threads_.reserve(n_threads);
        run();
    }

    ThreadPool::~ThreadPool()
    {
        stop_.test_and_set(std::memory_order::acq_rel);
        
        for (std::jthread& thread : threads_)
        {
            if (thread.joinable()) thread.join();
        }
    }

    void ThreadPool::run()
    {
        for (size_t i = threads_.capacity(); i--;)
        {
            threads_.emplace_back([this]()
                {
                    while (true)
                    {
                        std::optional<PriorityQueue::Task> maybe_task = 
                            p_queue_->try_pop();
    
                        if (maybe_task) try
                        {
                            (*maybe_task)();
                        }
                        catch(...)
                        {}
                        else if (stop_.test(std::memory_order::acquire)) break;
                    }
                });
        }
    }
}  // namespace dispatcher::thread_pool