#pragma once

#include "queue/queue.hpp"

#include <list>

#include <optional>
#include <functional>

#include <mutex>
#include <atomic>
#include <condition_variable>

namespace dispatcher::queue
{
    template <typename T>
    class UnboundedQueue : public IQueue<T>
    {
        std::list<T> queue_;

        mutable std::mutex queue_mutex_;
        bool drain_ = false;
        std::condition_variable not_empty_cv_;

    public:
        explicit UnboundedQueue(size_t capacity = 0)
        {}

        ~UnboundedQueue() override;

        void push(const T& element) override;
        void push(T&& element) override;

        std::optional<T> try_pop() override;

        bool empty() const override;
        void drain() override;
    };

    template <typename T>
    UnboundedQueue<T>::~UnboundedQueue() { drain(); }

    template <typename T>
    void UnboundedQueue<T>::push(const T& element)
    {
        std::unique_lock lock(queue_mutex_);
        queue_.emplace_back(element);

        not_empty_cv_.notify_one();
    }

    template <typename T>
    void UnboundedQueue<T>::push(T&& element)
    {
        std::unique_lock lock(queue_mutex_);
        queue_.emplace_back(std::move(element));

        not_empty_cv_.notify_one();
    }

    template <typename T>
    std::optional<T> UnboundedQueue<T>::try_pop()
    {
        std::unique_lock lock(queue_mutex_);
        not_empty_cv_.wait(lock, 
            [this]()
            {
                return queue_.size() || drain_;
            });
        
        if (!queue_.size()) return std::nullopt;

        std::optional<T> out{std::move(queue_.front())};
        queue_.pop_front();

        return out;
    }

    template <typename T>
    bool UnboundedQueue<T>::empty() const
    {
        std::lock_guard lock(queue_mutex_);
        return queue_.empty();
    }

    template <typename T>
    void UnboundedQueue<T>::drain()
    {
        {
            std::lock_guard lock(queue_mutex_);
            drain_ = true;
        }

        not_empty_cv_.notify_all();
    }
}  // namespace dispatcher::queue