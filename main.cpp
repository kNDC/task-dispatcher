#include "task_dispatcher.hpp"
#include "logger.hpp"

#include <iostream>
#include <print>

#include <climits>

#include <thread>

#include <chrono>

using namespace dispatcher;

int main()
{
    TaskDispatcher td(std::max(std::thread::hardware_concurrency() - 1, 1u));
    std::vector<std::jthread> threads;

    for (size_t i = 0; i < 5; ++i)
    {
        threads.emplace_back([&, i]()
        {
            for (size_t j = 0; j < 10; ++j)
            {
                td.schedule(TaskPriority::Normal, 
                    [=]()
                    {
                        Logger::Get().Log("Normal priority message №" + 
                            std::to_string(10 * i + j));
                    });
                
                td.schedule(TaskPriority::High, 
                    [=]()
                    {
                        Logger::Get().Log("High priority message №" + 
                            std::to_string(10 * i + j));
                    });
            }
        });
    }
}