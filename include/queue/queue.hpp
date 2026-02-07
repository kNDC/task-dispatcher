#pragma once

#include <functional>
#include <optional>

namespace dispatcher::queue
{
    struct QueueOptions
    {
        std::optional<int> capacity;
    };

    template <typename T>
    class IQueue
    {
    public:
        virtual ~IQueue() = default;

        virtual void push(T element) = 0;
        virtual std::optional<T> try_pop() = 0;

        virtual bool empty() const = 0;
        virtual void drain() = 0;
    };
}  // namespace dispatcher::queue