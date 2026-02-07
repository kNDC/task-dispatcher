#pragma once
#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"
#include "types.hpp"

#include <memory>

#include <array>
#include <map>

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <limits>
#include <stdexcept>

namespace dispatcher::queue
{
    class PriorityQueue
    {
    private:
        std::array<std::unique_ptr<IQueue<std::function<void()>>>, 
            n_priorities> queues_;
        
        std::mutex mutex_;
        std::condition_variable not_empty_cv_;
        std::atomic_flag drain_ = ATOMIC_FLAG_INIT;

    public:
        using Task = std::function<void()>;
        using Config = std::map<TaskPriority, QueueOptions>;

        explicit PriorityQueue(Config&& config);
        ~PriorityQueue();

        void push(TaskPriority priority, Task task);

        // Блокируется при пустой очереди до вызова drain(), 
        // а после выводит std::nullopt.
        std::optional<Task> try_pop();

        void drain();
    };

}  // namespace dispatcher::queue