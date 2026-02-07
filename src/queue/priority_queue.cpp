#include "queue/priority_queue.hpp"

namespace dispatcher::queue
{
    PriorityQueue::PriorityQueue(Config&& config)
    {
        if (config.size() != n_priorities)
        {
            throw std::logic_error("Unsupported configuration");
        }
        
        for (const std::pair<const TaskPriority, QueueOptions>& config_datum : config)
        {
            // Если задана ёмкость, используется ограниченная очередь
            if (config_datum.second)
            {
                size_t capacity = *config_datum.second;
                queues_[priorities.at(config_datum.first)] = 
                    std::make_unique<BoundedQueue<Task>>(capacity);
            }
            else // Иначе, неограниченная очередь
            {
                queues_[priorities.at(config_datum.first)] = 
                    std::make_unique<UnboundedQueue<Task>>();
            }

            /* Все очереди ставятся в пропускающий режим - 
            так при опустошении всех очередей потоки блоки-
            руются вне очередей */
            queues_[priorities.at(config_datum.first)]->drain();
        }
    }
    
    PriorityQueue::~PriorityQueue()
    {
        drain();
    }

    void PriorityQueue::push(TaskPriority priority, Task task)
    {
        queues_[priorities.at(priority)]->push(std::move(task));

        { std::unique_lock lock(mutex_); }
        not_empty_cv_.notify_one();
    }
    
    std::optional<PriorityQueue::Task> PriorityQueue::try_pop()
    {
        std::optional<Task> out{};

        // Проверка каждой очереди на наличие задач
        do
        {
            for (size_t i = 0; i < queues_.size(); ++i)
            {
                out = queues_[i]->try_pop();
                if (out) return out;
            }

            /* Со снятием блокировки нет смысла ждать 
            поступления задач в очереди */
            if (drain_.test(std::memory_order::acquire)) break;
            
            std::unique_lock lock(mutex_);
            not_empty_cv_.wait(lock, 
                [this]()
                {
                    if (drain_.test(std::memory_order::acquire)) return true;
                    
                    for (size_t i = 0; i < queues_.size(); ++i)
                    {
                        if (!queues_[i]->empty()) return true;
                    }
                    
                    return false;
                });
            
        } while (true);
        
        return out;
    }

    void PriorityQueue::drain()
    {
        {
            std::unique_lock lock(mutex_);
            drain_.test_and_set(std::memory_order::release);
        }

        /* Больше не нужно блокироваться из-за
        пустых очередей */
        not_empty_cv_.notify_all();
    }
} // namespace dispatcher::queue