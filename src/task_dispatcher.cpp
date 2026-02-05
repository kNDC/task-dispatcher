#include "task_dispatcher.hpp"

namespace dispatcher
{
    TaskDispatcher::TaskDispatcher(size_t n_threads, 
        PriorityQueue::Config&& config) : 
        p_queue_{std::make_shared<PriorityQueue>(std::move(config))}, 
        pool_{p_queue_, n_threads}
    {}

    void TaskDispatcher::schedule(TaskPriority priority, 
        std::function<void()> task)
    {
        p_queue_->push(priority, std::move(task));
    }

    TaskDispatcher::~TaskDispatcher()
    {
        p_queue_->shutdown();
    }
}  // namespace dispatcher