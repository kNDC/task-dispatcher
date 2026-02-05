#pragma once

#include "queue/queue.hpp"

#include <list>

#include <mutex>
#include <atomic>
#include <condition_variable>

namespace dispatcher::queue
{
    template <typename T>
    class BoundedQueue : public IQueue<T>
    {
    private:
        std::list<T> queue_;
        size_t capacity_ = 0;

        mutable std::mutex queue_mutex_;
        bool drain_ = false;
        std::condition_variable not_empty_msg_;
        std::condition_variable not_full_msg_;

    public:
        explicit BoundedQueue(size_t capacity);
        ~BoundedQueue() override;

        void push(T element) override;
        std::optional<T> try_pop() override;

        bool empty() const override;
        void drain() override;
    };
    
    template <typename T>
    BoundedQueue<T>::BoundedQueue(size_t capacity) : 
        capacity_(capacity)
    {}

    template <typename T>
    BoundedQueue<T>::~BoundedQueue() { drain(); }

    template <typename T>
    void BoundedQueue<T>::push(T element)
    {
        std::unique_lock lock(queue_mutex_);
        not_full_msg_.wait(lock, 
            [this]()
            {
                return queue_.size() < capacity_;
            });
        
        queue_.emplace_back(std::move(element));

        lock.unlock();
        not_empty_msg_.notify_one();
    }

    template <typename T>
    std::optional<T> BoundedQueue<T>::try_pop()
    {
        std::unique_lock lock(queue_mutex_);
        not_empty_msg_.wait(lock, 
            [this]()
            {
                return queue_.size() || drain_;
            });
        
        if (!queue_.size()) return std::nullopt;
        
        std::optional<T> out{std::move(queue_.front())};
        queue_.pop_front();

        lock.unlock();
        not_full_msg_.notify_one();

        return out;
    }

    template <typename T>
    bool BoundedQueue<T>::empty() const
    {
        std::lock_guard lock(queue_mutex_);
        return queue_.empty();
    }

    template <typename T>
    void BoundedQueue<T>::drain()
    {
        {
            std::lock_guard lock(queue_mutex_);
            drain_ = true;
        }

        not_empty_msg_.notify_all();
    }
}  // namespace dispatcher::queue